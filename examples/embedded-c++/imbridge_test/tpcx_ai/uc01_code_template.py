import joblib
import pyarrow as pa
import numpy as np
import pandas as pd
from sklearn.cluster import KMeans
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import MinMaxScaler

from threadpoolctl import threadpool_limits
@threadpool_limits.wrap(limits=1)
def process_table(table):
    scale = 40
    name = "uc01"
    root_model_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/tpcxai_datasets/sf{scale}"
    model_file_name = f"{root_model_path}/model/{name}/{name}.python.model"
    model = joblib.load(model_file_name)
    data = table.to_pandas().values
    feat = pd.DataFrame({
        'return_ratio': data[:, 0],
        'frequency': data[:, 1]
    })
    res = model.predict(feat)
    df = pd.DataFrame(res)
    # print(len(df))
    return pa.Table.from_pandas(df)

class MyProcess:
    def __init__(self):
        # load model part
        pass

    def process(self, table):
        # print(table.num_rows)
        return process_table(table)