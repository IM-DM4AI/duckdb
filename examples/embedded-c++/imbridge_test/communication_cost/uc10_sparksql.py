import sys
import pandas as pd
from pyspark.sql.functions import udf, pandas_udf, PandasUDFType
from pyspark.sql import SparkSession
from pyspark.sql.functions import udf
import time



con = SparkSession.builder.appName("session").config(
    "spark.executor.instance", "1").getOrCreate()

con.catalog.clearCache()

root_path = "/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/communication_cost"

name = "uc10_used_data"

df = con.read.csv(f"{root_path}/{name}.csv", header=True, inferSchema=True)
df.createOrReplaceTempView(name)


@pandas_udf('long', PandasUDFType.SCALAR)
def udf(business_hour_norm, amount_norm):
    return business_hour_norm



con.udf.register("udf", udf)

sql1 = f"""
select udf(amount_norm, business_hour_norm) 
FROM {name};
"""

sql2 = f"""
select amount_norm
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
