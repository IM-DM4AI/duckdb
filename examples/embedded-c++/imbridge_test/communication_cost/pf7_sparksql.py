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

name = "pf7_used_data"

df = con.read.csv(f"{root_path}/{name}.csv", header=True, inferSchema=True)
df.createOrReplaceTempView(name)


@pandas_udf('long', PandasUDFType.SCALAR)
def udf(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15, V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount)->pd.Series:
    return V1


con.udf.register("udf", udf)

sql1 = f"""
SELECT udf(V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13, V14, V15,
 V16, V17, V18, V19, V20, V21, V22, V23, V24, V25, V26, V27, V28, Amount)
FROM {name};
"""

sql2 = f"""
SELECT V1
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
