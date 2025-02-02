import warnings
warnings.filterwarnings('ignore')
import duckdb
import joblib
import numpy as np
import pandas as pd
from duckdb.typing import BIGINT, DOUBLE, FLOAT,VARCHAR
from statsmodels.tools.sm_exceptions import ValueWarning
from statsmodels.tsa.holtwinters import ExponentialSmoothing
import joblib
import time
from tqdm import tqdm
# hand_type = "udf"
hand_type = "special"
name = "uc03"
scale = 40


con = duckdb.connect(
    f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_tpcx_ai_sf{scale}.db")

root_model_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/tpcxai_datasets/sf{scale}"


model_file_name = f"{root_model_path}/model/{name}/{name}.python.model"

class UseCase03Model(object):

    def __init__(self, use_store=False, use_department=True):
        if not use_store and not use_department:
            raise ValueError(f"use_store = {use_store}, use_department = {use_department}: at least one must be True")

        self._use_store = use_store
        self._use_department = use_department
        self._models = {}
        self._min = {}
        self._max = {}

    def _get_key(self, store, department):
        if self._use_store and self._use_department:
            key = (store, department)
        elif self._use_store:
            key = store
        else:
            key = department

        return key

    def store_model(self, store: int, department: int, model, ts_min, ts_max):
        key = self._get_key(store, department)
        self._models[key] = model
        self._min[key] = ts_min
        self._max[key] = ts_max

    def get_model(self, store: int, department: int):
        key = self._get_key(store, department)
        model = self._models[key]
        ts_min = self._min[key]
        ts_max = self._max[key]
        return model, ts_min, ts_max

models = joblib.load(model_file_name)


def udf(store, department):
    forecasts = []
    data = pd.DataFrame({
        'store': store,
        'department': department
    })
    # print(data.shape)
    # combinations = np.unique(data[['Store', 'Dept']].values, axis=0)
    for index, row in data.iterrows():
        store = row.store
        dept = row.department
        periods = 52
        try:
            current_model, ts_min, ts_max = models.get_model(store, dept)
        except KeyError:
            continue
        # disable warnings that non-date index is returned from forecast
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=ValueWarning)
            forecast = current_model.forecast(periods)
            forecast = np.clip(forecast, a_min=0.0, a_max=None)  # replace negative forecasts
        start = pd.date_range(ts_max, periods=2)[1]
        forecast_idx = pd.date_range(start, periods=periods, freq='W-FRI')
        forecasts.append(
            str({'store': store, 'department': dept, 'date': forecast_idx, 'weekly_sales': forecast})
        )
    res = np.array(forecasts)
    return res


con.create_function("udf", udf, [BIGINT, VARCHAR], VARCHAR, type="arrow", null_handling=hand_type)


sql = '''
explain analyze  select store, department, udf(store, department) 
from (select store, department 
from Order_o Join Lineitem on Order_o.o_order_id = Lineitem.li_order_id
Join Product on li_product_id=p_product_id 
group by store,department);
'''

times = 5
min1 = 0
max1 = 0
res = 0
flag = True
for i in tqdm(range(times)):
    s = time.perf_counter()
    con.sql(sql)
    e = time.perf_counter()
    t = e-s
    print(f"{i+1} : {t}")
    res = res + t
    if flag:
        flag = False
        min1 = t
        max1 = t
    else:
        min1 = t if min1 > t else min1
        max1 = t if max1 < t else max1
print(f"min : {min1}")
print(f"max : {max1}")
res = res - min1 - max1
times = times - 2
print(f"{name}, {res/times}s ")
