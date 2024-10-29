import duckdb 
from google.oauth2 import service_account
from google.auth.transport.requests import Request


duckdb_con = duckdb.connect(':memory:', config = {"allow_unsigned_extensions": "true"})
duckdb_con.sql("install './build/release/extension/gsheets/gsheets.duckdb_extension'")
duckdb_con.sql("load gsheets")

def get_token_from_user_file(user_file_path):
    SCOPES = ["https://www.googleapis.com/auth/spreadsheets"]

    credentials = service_account.Credentials.from_service_account_file(
        user_file_path,
        scopes=SCOPES
        )

    request = Request()
    credentials.refresh(request)
    return credentials.token

def set_gsheet_secret(duckdb_con, user_file_path):
    token = get_token_from_user_file(user_file_path)
    duckdb_con.sql(f"create or replace secret gsheet_secret (TYPE gsheet, token '{token}')")

user_file_path = "credentials.json"
set_gsheet_secret(duckdb_con, user_file_path)

print(duckdb_con.sql("select * from read_gsheet('url_to_query', sheet:='sheet_to_query');"))