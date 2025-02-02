from pyspark.sql import SparkSession
from pyspark.sql.functions import udf, pandas_udf, PandasUDFType
import pyspark.sql.types as ps_types
from tqdm import tqdm
import time

import pickle

import duckdb
import numpy as np
from duckdb.typing import BIGINT, DOUBLE, FLOAT, VARCHAR, BOOLEAN
import pandas as pd
import time
from sklearn.preprocessing import OneHotEncoder
from sklearn.preprocessing import StandardScaler
import time


root_model_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/test_raven"
scaler_path = f'{root_model_path}/Expedia/expedia_standard_scale_model.pkl'
enc_path = f'{root_model_path}/Expedia/expedia_one_hot_encoder.pkl'
model_path = f'{root_model_path}/Expedia/expedia_dt_model.pkl'
with open(scaler_path, 'rb') as f:
    scaler = pickle.load(f)
with open(enc_path, 'rb') as f:
    enc = pickle.load(f)
with open(model_path, 'rb') as f:
    model = pickle.load(f)



con = SparkSession.builder.appName("session") \
     .config("spark.sql.execution.arrow.maxRecordsPerBatch", "1000").getOrCreate()

con.catalog.clearCache()

root_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/communication_cost"

name = "pf1_used_data"


times = 5
min1 = 0
max1 = 0
res = 0
flag = True
for i in tqdm(range(times)):
    s = time.perf_counter()
    df1 = con.read.csv(f"{root_path}/Expedia_R1_hotels.csv", header=True, inferSchema=True)
    df1.createOrReplaceTempView("Expedia_R1_hotels")

    df2 = con.read.csv(f"{root_path}/Expedia_R2_searches.csv", header=True, inferSchema=True)
    df2.createOrReplaceTempView("Expedia_R2_searches")

    df3 = con.read.csv(f"{root_path}/Expedia_S_listings_extension.csv", header=True, inferSchema=True)
    df3.createOrReplaceTempView("Expedia_S_listings_extension")

    @pandas_udf('long', PandasUDFType.SCALAR)
    def udf(prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
            orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
            position_, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
            year_, month_, weekofyear_, time_, site_id, visitor_location_country_id, srch_destination_id,
            srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
            srch_room_count, srch_saturday_night_bool, random_bool)->pd.Series:
        data = np.column_stack([prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
                                orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
                                position_, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
                                year_, month_, weekofyear_, time_, site_id, visitor_location_country_id,
                                srch_destination_id,
                                srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
                                srch_room_count, srch_saturday_night_bool, random_bool])
        data = np.split(data, np.array([8]), axis=1)
        numerical = scaler.transform(data[0])
        categorical = enc.transform(data[1]).toarray()
        X = np.hstack((numerical,
                    categorical))
        res = model.predict(X)
        print(len(prop_location_score1))
        return pd.Series(res)

    con.udf.register("udf", udf)

    sql2 = """
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


    
    con.sql(sql2).collect()
    e = time.perf_counter()
    t = e-s
    print(f"{i+1} : {t} s")
    res = res + t
    con.catalog.clearCache()
    if flag:
        flag = False
        min1 = t
        max1 = t
    else:
        min1 = t if min1 > t else min1
        max1 = t if max1 < t else max1
print(f"min : {min1}\nmax : {max1}")
res = res - min1 - max1
times = times - 2
print(f"{name}, {res/times}s ")

