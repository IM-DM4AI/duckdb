import pandas as pd
import pyarrow as pa
import pickle
import numpy as np

from sklearn.preprocessing import OneHotEncoder
from sklearn.preprocessing import StandardScaler
from threadpoolctl import threadpool_limits


limits_threads = 0
with open("/root/workspace/duckdb/.vscode/experiment_cfg/ml_thread.cfg", "r") as f:
    limits_threads = int(f.readline().strip())

@threadpool_limits.wrap(limits=limits_threads)
def process_table(table):
    root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
    scaler_path = f'{root_model_path}/Hospital/hospital_standard_scale_model.pkl'
    enc_path = f'{root_model_path}/Hospital/hospital_one_hot_encoder.pkl'
    model_path = f'{root_model_path}/Hospital/hospital_lr_model.pkl'
    with open(scaler_path, 'rb') as f:
        scaler = pickle.load(f)
    with open(enc_path, 'rb') as f:
        enc = pickle.load(f)
    with open(model_path, 'rb') as f:
        model = pickle.load(f)

    data = table.to_pandas().values
    data = np.split(data, np.array([10]), axis=1)
    numerical = data[0]
    categorical = data[1]
    
    X = np.hstack((scaler.transform(numerical), enc.transform(categorical).toarray()))
    res = model.predict(X)
    df = pd.DataFrame(res)
    return pa.Table.from_pandas(df)
    

class MyProcess:
    def __init__(self):
        # load model part
        pass


    def process(self, table):
        # print(table.num_rows)
        return process_table(table)