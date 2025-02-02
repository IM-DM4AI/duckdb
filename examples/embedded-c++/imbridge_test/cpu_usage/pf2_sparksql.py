from pyspark.sql import SparkSession
from pyspark.sql.functions import udf, pandas_udf, PandasUDFType
import pickle

import duckdb
import numpy as np
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR, BOOLEAN
import pandas as pd
import time
import onnxruntime as ort
import time
from tqdm import tqdm

con = SparkSession.builder.appName("session").config(
    "spark.executor.instance", "1").getOrCreate()

con.catalog.clearCache()

root_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/communication_cost"

table_name = ["Expedia_S_listings_extension", "Expedia_R1_hotels", "Expedia_R2_searches"]


for table in table_name:
    df = con.read.csv(f"{root_path}/{table}.csv", header=True, inferSchema=True)
    df.createOrReplaceTempView(table)

root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
onnx_path = f'{root_model_path}/Expedia/expedia_dt_pipeline.onnx'


@pandas_udf('long', PandasUDFType.SCALAR)
def udf(prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
        orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
        position_, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
        year_, month_, weekofyear_, time_, site_id, visitor_location_country_id, srch_destination_id,
        srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
        srch_room_count, srch_saturday_night_bool, random_bool):
    ortconfig = ort.SessionOptions()
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

    return pd.Series(udf_wrap(prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
        orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
        position_, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
        year_, month_, weekofyear_, time_, site_id, visitor_location_country_id, srch_destination_id,
        srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
        srch_room_count, srch_saturday_night_bool, random_bool))



con.udf.register("udf", udf)

print("start!!!!!!!!!!!!!!!!!!!!!!!")

time.sleep(4)

sql1 = f"""
SELECT udf(prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
                           orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
                           position, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
                           year, month, weekofyear, time, site_id, visitor_location_country_id, srch_destination_id,
                           srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
                           srch_room_count, srch_saturday_night_bool, random_bool) 
FROM Expedia_S_listings_extension JOIN Expedia_R1_hotels ON Expedia_S_listings_extension.prop_id = Expedia_R1_hotels.prop_id 
JOIN Expedia_R2_searches ON Expedia_S_listings_extension.srch_id = Expedia_R2_searches.srch_id WHERE prop_location_score1 > 1 and prop_location_score2 > 0.1 
and prop_log_historical_price > 4 and count_bookings > 5 
and srch_booking_window > 10 and srch_length_of_stay > 1;
"""


s = time.perf_counter()
con.sql(sql1).collect()
e = time.perf_counter()
print(e-s)


