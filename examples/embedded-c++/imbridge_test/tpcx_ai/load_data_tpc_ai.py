import duckdb
name = "tpcx_ai"

# only 10, 30, 40, 60, 100
scale = 40

data_root_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/tpcxai_datasets/sf{scale}/serving"

db_root_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/db/db_{name}_sf{scale}.db"


con = duckdb.connect(db_root_path)

con.sql("CREATE TABLE IF NOT EXISTS "
        "Order_Returns(or_order_id INTEGER, or_product_id INTEGER, or_return_quantity INTEGER)")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Order_o(o_order_id INTEGER, o_customer_sk INTEGER, weekday VARCHAR(32), date VARCHAR(32), store INTEGER)")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Lineitem(li_order_id INTEGER, li_product_id INTEGER, quantity INTEGER, price DOUBLE)")

# con.sql("CREATE TABLE IF NOT EXISTS "
#         "Lineitem_extension(idx INTEGER, li_order_id INTEGER, li_product_id INTEGER, quantity INTEGER, price DOUBLE)")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Product(p_product_id INTEGER, name VARCHAR(32), department VARCHAR(32))")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Review (ID INTEGER, text VARCHAR(65535))")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Product_Rating (userID INTEGER, productID INTEGER)")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Failures (date VARCHAR(32), serial_number INTEGER, model VARCHAR(32), smart_5_raw DOUBLE, smart_10_raw DOUBLE, smart_184_raw DOUBLE, smart_187_raw DOUBLE, smart_188_raw DOUBLE, smart_197_raw DOUBLE, smart_198_raw DOUBLE)")

# con.sql("CREATE TABLE IF NOT EXISTS "
#         "Failures_extension(idx INTEGER, date VARCHAR(32), serial_number INTEGER, model VARCHAR(32), smart_5_raw DOUBLE, smart_10_raw DOUBLE, smart_184_raw DOUBLE, smart_187_raw DOUBLE, smart_188_raw DOUBLE, smart_197_raw DOUBLE, smart_198_raw DOUBLE)")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Financial_Account (fa_customer_sk INTEGER, transaction_limit DOUBLE)")

con.sql("CREATE TABLE IF NOT EXISTS "
        "Financial_Transactions (amount DOUBLE, IBAN VARCHAR(32), senderID INTEGER, receiverID VARCHAR(32), transactionID VARCHAR(32), time VARCHAR(32))")

# con.sql("CREATE TABLE IF NOT EXISTS "
#        "Financial_Transactions_extension(idx INTEGER, amount DOUBLE, IBAN VARCHAR(32), senderID INTEGER, receiverID VARCHAR(32), transactionID VARCHAR(32), time VARCHAR(32))")


print("Order_Returns")
con.sql(f"INSERT INTO Order_Returns (SELECT * FROM read_csv_auto('{data_root_path}/order_returns.csv') )")
print(con.sql("SELECT count(*) FROM Order_Returns"))

print("Order_o")
con.sql(f"INSERT INTO Order_o (SELECT * FROM read_csv_auto('{data_root_path}/order.csv') )")
print(con.sql("SELECT count(*) FROM Order_o"))

print("Lineitem")
con.sql(f"INSERT INTO Lineitem (SELECT * FROM read_csv_auto('{data_root_path}/lineitem.csv') )")
print(con.sql("SELECT count(*) FROM Lineitem"))

# con.sql("INSERT INTO Lineitem_extension SELECT * FROM read_csv_auto('{data_root_path}/lineitem_extension.csv')")
print("Product")
con.sql(f"INSERT INTO Product (SELECT * FROM read_csv_auto('{data_root_path}/product.csv') )")
print(con.sql("SELECT count(*) FROM Product"))

print("Review")
con.sql(f"INSERT INTO Review (SELECT * FROM read_csv_auto('{data_root_path}/Review.psv', delim = '|', header = true) )")
print(con.sql("SELECT count(*) FROM Review"))

print("Product_Rating")
con.sql(f"INSERT INTO Product_Rating (SELECT * FROM read_csv_auto('{data_root_path}/ProductRating.csv') )")
print(con.sql("SELECT count(*) FROM Product_Rating"))

print("Failures")
con.sql(f"INSERT INTO Failures (SELECT * FROM read_csv_auto('{data_root_path}/failures.csv') )")
print(con.sql("SELECT count(*) FROM Failures"))

# con.sql("INSERT INTO Failures_extension SELECT * FROM read_csv_auto('{data_root_path}/failures_extension.csv')")
print("Financial_Account")
con.sql(f"INSERT INTO Financial_Account (SELECT * FROM read_csv_auto('{data_root_path}/financial_account.csv') )")
print(con.sql("SELECT count(*) FROM Financial_Account"))

print("Financial_Transactions")
con.sql(f"INSERT INTO Financial_Transactions (SELECT * FROM read_csv_auto('{data_root_path}/financial_transactions.csv') )")
print(con.sql("SELECT count(*) FROM Financial_Transactions"))

# con.sql("INSERT INTO Financial_Transactions_extension SELECT * FROM read_csv_auto('{data_root_path}/financial_transaction_extension.csv')")



con.close()
