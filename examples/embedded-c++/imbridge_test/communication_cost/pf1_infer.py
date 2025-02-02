import pickle

import duckdb
import numpy as np
from duckdb.typing import BIGINT, DOUBLE, VARCHAR, BOOLEAN
import pandas as pd
import time
from sklearn.preprocessing import OneHotEncoder
from sklearn.preprocessing import StandardScaler
import time
import sys
from tqdm import tqdm
# hand_type = "udf"
hand_type = "special"
name = "pf1"

con = duckdb.connect(
    "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven.db")

# root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
# scaler_path = f'{root_model_path}/Expedia/expedia_standard_scale_model.pkl'
# enc_path = f'{root_model_path}/Expedia/expedia_one_hot_encoder.pkl'
# model_path = f'{root_model_path}/Expedia/expedia_dt_model.pkl'
# with open(scaler_path, 'rb') as f:
#     scaler = pickle.load(f)
# with open(enc_path, 'rb') as f:
#     enc = pickle.load(f)
# with open(model_path, 'rb') as f:
#     model = pickle.load(f)


# def udf(data):
#     data = np.split(data, np.array([8]), axis=1)
#     numerical = scaler.transform(data[0])
#     categorical = enc.transform(data[1]).toarray()
#     X = np.hstack((numerical,
#                    categorical))
#     res = model.predict(X)
#     return res


sql1 = '''
SELECT   prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
         orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
         position, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
         year, month, weekofyear, time, site_id, visitor_location_country_id, srch_destination_id,
         srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
         srch_room_count, srch_saturday_night_bool, random_bool
FROM pf1_used_data;
'''

sql2 = '''
explain analyze  SELECT   prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
         orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
         position, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
         year, month, weekofyear, time, site_id, visitor_location_country_id, srch_destination_id,
         srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
         srch_room_count, srch_saturday_night_bool, random_bool
FROM pf1_used_data;
'''

if sys.argv[1] == "udf":
    s = time.perf_counter()
    res_data = con.sql(sql1).fetch_arrow_table()
    # udf(*res_data)
    e = time.perf_counter()
    t = e-s
    print(f"udf : {t}")
else:
    s = time.perf_counter()
    res_data = con.sql(sql2)
    # udf(*res_data)
    e = time.perf_counter()
    t = e-s
    print(f"origin : {t}")

