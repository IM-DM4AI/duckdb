import pickle

import duckdb
import numpy as np
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR, BOOLEAN
import pandas as pd
import time
import onnxruntime as ort
import time
from tqdm import tqdm
# hand_type = "udf"
hand_type = "special"
name = "pf2"

root_data_path = ""

con = duckdb.connect(
    "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_raven_10G.db")

root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"


onnx_path = f'{root_model_path}/Expedia/expedia_dt_pipeline.onnx'
s1 = time.perf_counter()
ortconfig = ort.SessionOptions()
expedia_onnx_session = ort.InferenceSession(onnx_path, sess_options=ortconfig)
expedia_label = expedia_onnx_session.get_outputs()[0]
e1 = time.perf_counter()
model_load_time = e1-s1
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


def udf(prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
        orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
        position_, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
        year_, month_, weekofyear_, time_, site_id, visitor_location_country_id, srch_destination_id,
        srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
        srch_room_count, srch_saturday_night_bool, random_bool):

    def udf_wrap(*args):
        infer_batch = {
            elem: np.array(args[i]).astype(expedia_type_map[args[i].to_numpy().dtype.name]).reshape((-1, 1))
            for i, elem in enumerate(expedia_input_columns)
        }
        outputs = expedia_onnx_session.run([expedia_label.name], infer_batch)
        return outputs[0]

    return udf_wrap(prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
        orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
        position_, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
        year_, month_, weekofyear_, time_, site_id, visitor_location_country_id, srch_destination_id,
        srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
        srch_room_count, srch_saturday_night_bool, random_bool)



con.create_function("udf", udf,
                    [DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, DOUBLE, VARCHAR, VARCHAR, BIGINT, BOOLEAN, BIGINT, BIGINT, VARCHAR, VARCHAR, VARCHAR, VARCHAR, VARCHAR, VARCHAR, VARCHAR, BIGINT, BIGINT,BIGINT, BIGINT, BIGINT, BOOLEAN, BOOLEAN], BIGINT, type="arrow", null_handling=hand_type)


sql = '''
explain analyze SELECT Expedia_S_listings_extension.prop_id, Expedia_S_listings_extension.srch_id, udf(prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
                           orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
                           position, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
                           year, month, weekofyear, time, site_id, visitor_location_country_id, srch_destination_id,
                           srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
                           srch_room_count, srch_saturday_night_bool, random_bool)
FROM Expedia_S_listings_extension JOIN Expedia_R1_hotels ON Expedia_S_listings_extension.prop_id = Expedia_R1_hotels.prop_id
JOIN Expedia_R2_searches ON Expedia_S_listings_extension.srch_id = Expedia_R2_searches.srch_id WHERE prop_location_score1 > 1 and prop_location_score2 > 0.1
and prop_log_historical_price > 4 and count_bookings > 5
and srch_booking_window > 10 and srch_length_of_stay > 1;
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
    t = e-s+model_load_time
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
