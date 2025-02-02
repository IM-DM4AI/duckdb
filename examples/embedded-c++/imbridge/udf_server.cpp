#include "duckdb.hpp"
#include "duckdb/common/arrow/arrow_transform_util.hpp"
#include "imlane/scheduler/scheduler.hpp"

#include <arrow/python/pyarrow.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace duckdb;
using namespace imbridge;

int main(int argc, char **argv) {

	if (argc < 2) {
		std::cout << "[Server] you shoule add a parameter\n";
		return 0;
	}
	std::string channel_name = argv[1];
	int channel_name_int = std::atoi(channel_name.c_str());
	int cmd;
	bool stop_flag = true;
	// std::cout << "[Server] start " << channel_name << " server\n";
	if (!Py_IsInitialized()) {
		Py_Initialize();
	}
	PyGILState_STATE gstate;
	gstate = PyGILState_Ensure();

	PyObject *dycacher = PyImport_ImportModule("dycacher");
	if (!dycacher) {
		PyErr_Print();
	}

	PyObject *reset_func = PyObject_GetAttrString(dycacher, "reset_cache");
	if (!reset_func) {
		PyErr_Print();
		Py_DECREF(dycacher);
	}

	if (arrow::py::import_pyarrow()) {
		std::cout
		    << "[Server] import pyarrow error! make sure your default python environment has installed the pyarrow\n";
		exit(0);
	}

	imbridge::IMLaneScheduler scheduler(false, 0, channel_name_int);
	imbridge::SharedMemoryManager shm_server(channel_name, imbridge::ProcessKind::MANAGER);

	// prepare the environment
	std::ifstream file("/root/workspace/duckdb/examples/embedded-c++/imbridge/code.py");
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string python_code = buffer.str();
	PyRun_SimpleString(python_code.c_str());
	PyObject *main_module = PyImport_AddModule("__main__");
	PyObject *main_dict = PyModule_GetDict(main_module);
	PyObject *MyProcess = PyDict_GetItemString(main_dict, "MyProcess");
	PyObject *my_process_instance = PyObject_CallObject(MyProcess, NULL);
	if (my_process_instance == NULL) {
		PyErr_Print();
		return 0;
	}
	scheduler.push_id_to_avaliable_queue(channel_name_int);
	// std::cout << "[Server] prepare the environment   " << channel_name << std::endl;
	while (stop_flag) {
		cmd = scheduler.get_message_from_task_queue();
		switch (cmd) {
		case imbridge::TASK_UDF_INFER: {
			// std::cout << "[Server] start handle\n";
			// read table
			std::shared_ptr<arrow::Table> my_table = imbridge::ReadArrowTableFromSharedMemory(shm_server, INPUT_TABLE);

			// handle table and predict
			PyObject *py_table_tmp = arrow::py::wrap_table(std::move(my_table));
			PyObject *py_result = PyObject_CallMethod(my_process_instance, "process", "O", py_table_tmp);

			// get the result
			if (py_result != NULL) {
				my_table = arrow::py::unwrap_table(py_result).ValueOrDie();
			} else {
				PyErr_Print();
				return 0;
			}
			// std::cout << "[Server] handle finished\n";
			// write result to shared memory
			imbridge::WriteArrowTableToSharedMemory(my_table, shm_server, OUTPUT_TABLE);
			shm_server.sem_client->post();
			// TODO: update the avaliable queue
			shm_server.sem_server->wait();
			// std::cout << "[Server] end id : " << channel_name << std::endl;
			scheduler.push_id_to_avaliable_queue(channel_name_int);
			break;
		}
		case imbridge::TASK_DESTROY: {
			stop_flag = false;
			break;
		}
		case imbridge::TASK_RESET_CACHE: {
			PyObject *res = PyObject_CallObject(reset_func, NULL);
			if (!res) {
				PyErr_Print();
			}
			break;
		}
		default:
			throw std::invalid_argument("[Server] Invalid command " + std::to_string(cmd));
		}
	}
	// std::cout << "[Server] udf server " << channel_name << " closed\n";
	PyGILState_Release(gstate);
	Py_Finalize();
	return 0;
}