import pyarrow as pa
import numpy as np
import pandas as pd
import onnxruntime as ort
from threadpoolctl import threadpool_limits

limits_threads = 0
with open("/root/workspace/duckdb/.vscode/experiment_cfg/ml_thread.cfg", "r") as f:
    limits_threads = int(f.readline().strip())


@threadpool_limits.wrap(limits=limits_threads)
def process_table(table):
    root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
    onnx_path = f'{root_model_path}/Hospital/hospital_mlp_pipeline.onnx'
    core = limits_threads
    ortconfig = ort.SessionOptions()
    ortconfig.inter_op_num_threads = core
    ortconfig.intra_op_num_threads = 1
    hospital_onnx_session = ort.InferenceSession(onnx_path, sess_options=ortconfig)
    hospital_label = hospital_onnx_session.get_outputs()[0]
    numerical_columns = ['hematocrit', 'neutrophils', 'sodium', 'glucose', 'bloodureanitro', 'creatinine', 'bmi', 'pulse',
                        'respiration', 'secondarydiagnosisnonicd9']
    categorical_columns = ['rcount', 'gender', 'dialysisrenalendstage', 'asthma', 'irondef', 'pneum', 'substancedependence',
                        'psychologicaldisordermajor', 'depress', 'psychother', 'fibrosisandother', 'malnutrition',
                        'hemo']
    hospital_input_columns = numerical_columns + categorical_columns
    hospital_type_map = {
        'int32': np.int64,
        'int64': np.int64,
        'float64': np.float32,
        'object': str,
    }

    def udf_wrap(*args):
        infer_batch = {
            elem: args[i].to_numpy().astype(hospital_type_map[args[i].to_numpy().dtype.name]).reshape((-1, 1))
            for i, elem in enumerate(hospital_input_columns)
        }
        outputs = hospital_onnx_session.run([hospital_label.name], infer_batch)
        return outputs[0]
    df = pd.DataFrame(udf_wrap(*table))
    # print(len(df))
    return pa.Table.from_pandas(df)


class MyProcess:
    def __init__(self):
        # load model part
        pass

    def process(self, table):
        # print(table.num_rows)
        return process_table(table)