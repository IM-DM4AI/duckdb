import pickle
import pyarrow as pa
import numpy as np
import pandas as pd
from sklearn.preprocessing import OneHotEncoder
from sklearn.preprocessing import StandardScaler
import lightgbm as lgb


from threadpoolctl import threadpool_limits
@threadpool_limits.wrap(limits=1)
def process_table(table):
    root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
    scaler_path = f'{root_model_path}/Credit_Card/creditcard_standard_scale_model.pkl'
    model_path =  f'{root_model_path}/Credit_Card/creditcard_lgb_model.txt'

    with open(scaler_path, 'rb') as f:
        scaler = pickle.load(f)
    model = lgb.Booster(model_file=model_path)

    def udf(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15, V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount):
        data = np.column_stack(
            [V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15, V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount])
        numerical = np.column_stack(data.T)
        
        X = scaler.transform(numerical)
        # print(X.shape)
        return model.predict(X)
    df = pd.DataFrame(udf(*table))
    # print(len(df))
    return pa.Table.from_pandas(df)


class MyProcess:
    def __init__(self):
        # load model part
        pass

    def process(self, table):
        # print(table.num_rows)
        return process_table(table)