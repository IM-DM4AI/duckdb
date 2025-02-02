import pyarrow as pa
import numpy as np
import pandas as pd
import onnxruntime as ort

from threadpoolctl import threadpool_limits
@threadpool_limits.wrap(limits=1)
def process_table(table):
    root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
    onnx_path = f'{root_model_path}/Expedia/expedia_dt_pipeline.onnx'
    core = 1
    ortconfig = ort.SessionOptions()
    ortconfig.inter_op_num_threads = core
    ortconfig.intra_op_num_threads = core
    expedia_onnx_session = ort.InferenceSession(onnx_path, sess_options=ortconfig)
    expedia_label = expedia_onnx_session.get_outputs()[0]
    numerical_columns = ['prop_location_score1', 'prop_location_score2', 'prop_log_historical_price', 'price_usd', 'orig_destination_distance', 'prop_review_score', 'avg_bookings_usd', 'stdev_bookings_usd']
    categorical_columns = ['position', 'prop_country_id', 'prop_starrating', 'prop_brand_bool', 'count_clicks','count_bookings', 'year', 'month', 'weekofyear', 'time', 'site_id', 'visitor_location_country_id', 'srch_destination_id', 'srch_length_of_stay', 'srch_booking_window', 'srch_adults_count', 'srch_children_count', 'srch_room_count', 'srch_saturday_night_bool', 'random_bool']
    expedia_input_columns = numerical_columns + categorical_columns
    expedia_type_map = {
        'bool': np.int64,
        'int32': np.int64,
        'int64': np.int64,
        'float64': np.float32,
        'object': str,
    }
    
    def udf_wrap(*args):
        infer_batch = {
            elem: np.array(args[i]).astype(expedia_type_map[args[i].to_numpy().dtype.name]).reshape((-1, 1))
            for i, elem in enumerate(expedia_input_columns)
        }
        outputs = expedia_onnx_session.run([expedia_label.name], infer_batch)
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