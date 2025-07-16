
import duckdb
from duckdb.typing import BIGINT, DOUBLE, FLOAT,VARCHAR
from duckdb.functional import PythonUDFKind



con = duckdb.connect(
    f"/home/duckdb/examples/embedded-c++/imbridge_test/db/db_tpcx_ai_sf40.db")


def udf(store, department):
    return department


con.create_function("udf", udf, [BIGINT, VARCHAR], VARCHAR, type="arrow", kind=PythonUDFKind.PROCESS_PREDICTION)


sql = '''
select store, department, udf(store, department) 
from (select store, department 
from Order_o Join Lineitem on Order_o.o_order_id = Lineitem.li_order_id
Join Product on li_product_id=p_product_id 
group by store,department);
'''
print(con.sql(sql).explain())

