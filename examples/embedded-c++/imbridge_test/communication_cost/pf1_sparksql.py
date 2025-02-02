from pyspark.sql import SparkSession
from pyspark.sql.functions import udf, pandas_udf, PandasUDFType
import pandas as pd
import sys
import time

con = SparkSession.builder.appName("session").config(
    "spark.executor.instance", "1").getOrCreate()

con.catalog.clearCache()

root_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/communication_cost"

name = "pf1_used_data"

df = con.read.csv(f"{root_path}/{name}.csv", header=True, inferSchema=True)
df.createOrReplaceTempView(name)


@pandas_udf('long', PandasUDFType.SCALAR)
def udf(prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
        orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
        position_, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
        year_, month_, weekofyear_, time_, site_id, visitor_location_country_id, srch_destination_id,
        srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
        srch_room_count, srch_saturday_night_bool, random_bool) -> pd.Series:
    return srch_room_count


con.udf.register("udf", udf)

sql1 = f"""
SELECT udf(prop_location_score1, prop_location_score2, prop_log_historical_price, price_usd,
                           orig_destination_distance, prop_review_score, avg_bookings_usd, stdev_bookings_usd,
                           position, prop_country_id, prop_starrating, prop_brand_bool, count_clicks, count_bookings,
                           year, month, weekofyear, time, site_id, visitor_location_country_id, srch_destination_id,
                           srch_length_of_stay, srch_booking_window, srch_adults_count, srch_children_count,
                           srch_room_count, srch_saturday_night_bool, random_bool) 
FROM {name};
"""

sql2 = f"""
SELECT srch_room_count
FROM {name};
"""



if sys.argv[1] == "udf":
    s = time.perf_counter()
    con.sql(sql1).collect()
    e = time.perf_counter()
    t = e-s
    print(f"udf : {t}")
else:
    s = time.perf_counter()
    con.sql(sql2).collect()
    e = time.perf_counter()
    t = e-s
    print(f"origin : {t}")



