import pyarrow as pa
import joblib
import numpy as np
import pandas as pd
from scipy.sparse import csr_matrix
from xgboost.sklearn import XGBClassifier


def process_table(table):
    scale = 10
    name = "uc08"
    root_model_path = f"/root/workspace/duckdb/examples/embedded-c++/imbridge_test/data/tpcxai_datasets/sf{scale}"
    model_file_name = f"{root_model_path}/model/{name}/{name}.python.model"
    model = joblib.load(model_file_name)
    label_range = [3, 4, 5, 6, 7, 8, 9, 12, 15, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
                    37, 38, 39, 40, 41, 42, 43, 44, 999]
    sorted_labels = sorted(label_range, key=str)

    def decode_label(label):
        return sorted_labels[label]

    def udf(scan_count, scan_count_abs, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday, dep0, dep1, dep2,
            dep3, dep4, dep5, dep6, dep7,
            dep8, dep9, dep10, dep11, dep12, dep13, dep14, dep15, dep16, dep17, dep18, dep19, dep20, dep21, dep22, dep23,
            dep24, dep25, dep26,
            dep27, dep28, dep29, dep30, dep31, dep32, dep33, dep34, dep35, dep36, dep37, dep38, dep39, dep40, dep41, dep42,
            dep43, dep44, dep45,
            dep46, dep47, dep48, dep49, dep50, dep51, dep52, dep53, dep54, dep55, dep56, dep57, dep58, dep59, dep60, dep61,
            dep62, dep63, dep64,
            dep65, dep66, dep67):
        data = pd.DataFrame({"scan_count": scan_count, "scan_count_abs": scan_count_abs, "Monday": Monday,
                            "Tuesday": Tuesday, "Wednesday": Wednesday, "Thursday": Thursday,
                                "Friday": Friday, "Saturday": Saturday, "Sunday": Sunday,
                                'dep0': dep0, 'dep1': dep1, 'dep2': dep2, 'dep3': dep3, 'dep4': dep4, 'dep5': dep5, 'dep6': dep6,
                                'dep7': dep7, 'dep8': dep8, 'dep9': dep9, 'dep10': dep10, 'dep11': dep11, 'dep12': dep12, 'dep13': dep13,
                                'dep14': dep14, 'dep15': dep15, 'dep16': dep16, 'dep17': dep17, 'dep18': dep18, 'dep19': dep19,
                                'dep20': dep20, 'dep21': dep21, 'dep22': dep22, 'dep23': dep23, 'dep24': dep24, 'dep25': dep25,
                                'dep26': dep26, 'dep27': dep27, 'dep28': dep28, 'dep29': dep29, 'dep30': dep30, 'dep31': dep31,
                                'dep32': dep32, 'dep33': dep33, 'dep34': dep34, 'dep35': dep35, 'dep36': dep36, 'dep37': dep37,
                                'dep38': dep38, 'dep39': dep39, 'dep40': dep40, 'dep41': dep41, 'dep42': dep42, 'dep43': dep43,
                                'dep44': dep44, 'dep45': dep45, 'dep46': dep46, 'dep47': dep47, 'dep48': dep48, 'dep49': dep49,
                                'dep50': dep50, 'dep51': dep51, 'dep52': dep52, 'dep53': dep53, 'dep54': dep54, 'dep55': dep55,
                                'dep56': dep56, 'dep57': dep57, 'dep58': dep58, 'dep59': dep59, 'dep60': dep60, 'dep61': dep61,
                                'dep62': dep62, 'dep63': dep63, 'dep64': dep64, 'dep65': dep65, 'dep66': dep66, 'dep67': dep67})
        sparse_data = csr_matrix(data)
        predictions = model.predict(sparse_data)
        dec_fun = np.vectorize(decode_label)

        return dec_fun(predictions)
    df = pd.DataFrame(udf(*table))
    # print(len(df))
    return pa.Table.from_pandas(df)

class MyProcess:
    def __init__(self):
        # load model part
        pass

    def process(self, table):
        # print(table.num_rows)
        return process_table(table)

