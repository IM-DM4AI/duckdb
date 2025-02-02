import pickle
import pyarrow as pa
import numpy as np
import pandas as pd
from sklearn.preprocessing import OneHotEncoder
from sklearn.preprocessing import StandardScaler
from catboost import CatBoostClassifier
from threadpoolctl import threadpool_limits

class MyProcess:
    def __init__(self):
        # load model part
        root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
        scaler_path = f'{root_model_path}/Credit_Card/creditcard_standard_scale_model.pkl'
        model_path = f'{root_model_path}/Credit_Card/creditcard_catboost_gb.cbm'
        with open(scaler_path, 'rb') as f:
            self.scaler = pickle.load(f)
        self.model = CatBoostClassifier()
        self.model.load_model(model_path)

    
    @threadpool_limits.wrap(limits=1)
    def process(self, table):
        # print(table.num_rows)
        data = table.to_pandas().values
        numerical = np.column_stack(data.T)
        X = self.scaler.transform(numerical)
        res = self.model.predict(X)
        df = pd.DataFrame(res)
        return pa.Table.from_pandas(df)