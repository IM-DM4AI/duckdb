from tqdm import tqdm
import time
from sklearn.preprocessing import MinMaxScaler
from sklearn.pipeline import Pipeline
from sklearn.cluster import KMeans
import pandas as pd
import numpy as np
import joblib
import duckdb
import os
os.environ['NUM_THREADS'] = '96'
# K-Means clustering

# hand_type = "udf"
hand_type = "special"
name = "uc01"
scale = 40


con = duckdb.connect(
    f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_tpcx_ai_sf{scale}.db")

root_model_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/tpcxai_datasets/sf{scale}"


model_file_name = f"{root_model_path}/model/{name}/{name}.python.model"

model_load_time = 0

s1 = time.perf_counter()
model = joblib.load(model_file_name)
e1 = time.perf_counter()

model_load_time = e1-s1



def udf(return_ratio, frequency):
    feat = pd.DataFrame({
        'return_ratio': return_ratio,
        'frequency': frequency
    })
    res = []
    total_len = feat.shape[0]
    bs = 4096
    times = total_len//bs
    for i in range(times):
        res.append(model.predict(feat.iloc[i*bs:(i+1)*bs, :]))
    res.append(model.predict(feat.iloc[times*bs:, :]))
    return res


sql='''
select  COALESCE(return_ratio,0), COALESCE(frequency,0)
from (select o_customer_sk, mean(ratio) return_ratio
from (select o_customer_sk, return_row_price_sum/row_price_sum ratio
from (select o_order_id, o_customer_sk, SUM(row_price) row_price_sum, SUM(return_row_price) return_row_price_sum, SUM(invoice_year) invoice_year_min
from (select o_order_id, o_customer_sk, extract('year' FROM cast(date0 as DATE)) invoice_year, quantity*price row_price, or_return_quantity*price return_row_price
from (select o_order_id, o_customer_sk, date date0, li_product_id, price, quantity, or_return_quantity
from (select * from lineitem left join Order_Returns
on lineitem.li_order_id = Order_Returns.or_order_id
and lineitem.li_product_id = Order_Returns.or_product_id
) returns_data Join Order_o on returns_data.li_order_id=Order_o.o_order_id))
group by o_order_id, o_customer_sk)
)
group by o_customer_sk
) ratio_tbl
join (select o_customer_sk, mean(o_order_id) frequency
from (select o_customer_sk, invoice_year_min, count(DISTINCT o_order_id) o_order_id
from (select o_order_id, o_customer_sk, SUM(row_price) row_price_sum, SUM(return_row_price) return_row_price_sum, SUM(invoice_year) invoice_year_min
from (select o_order_id, o_customer_sk, extract('year' FROM cast(date0 as DATE)) invoice_year, quantity*price row_price, or_return_quantity*price return_row_price
from (select o_order_id, o_customer_sk, date date0, li_product_id, price, quantity, or_return_quantity
from (select * from lineitem left join Order_Returns
on lineitem.li_order_id = Order_Returns.or_order_id
and lineitem.li_product_id = Order_Returns.or_product_id
) returns_data Join Order_o on returns_data.li_order_id=Order_o.o_order_id))
group by o_order_id, o_customer_sk
)
group by o_customer_sk, invoice_year_min
) group by o_customer_sk
) frequency_tbl on ratio_tbl.o_customer_sk = frequency_tbl.o_customer_sk;
'''
times=5
min1=0
max1=0
res=0
flag=True
for i in tqdm(range(times)):
    s=time.perf_counter()
    res_data=con.sql(sql).fetch_arrow_table()
    udf(*res_data)
    e=time.perf_counter()
    t=e-s+model_load_time
    print(f"{i+1} : {t}")
    res=res + t
    if flag:
        flag=False
        min1=t
        max1=t
    else:
        min1=t if min1 > t else min1
        max1=t if max1 < t else max1
print(f"min : {min1}")
print(f"max : {max1}")
res=res - min1 - max1
times=times - 2
print(f"{name}, {res/times}s ")
