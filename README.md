**🚧 WARNING - Work in Progress**: Here be many dragons 🚧

# DuckDB GSheets

This extension, GSheets, allows you to read and write to Google Sheets using DuckDB.

## Usage 

```sql
-- Create a secret with your Google API access token
CREATE SECRET test_secret (type gsheet, token '<your_token>');

-- Read a sheet by sheet id
SELECT * FROM read_gsheet('11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8');

-- Read a sheet by full URL
SELECT * FROM read_gsheet('https://docs.google.com/spreadsheets/d/11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8/edit?usp=sharing');

-- Read a sheet with no header row
SELECT * FROM read_gsheet('11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8', headers=false);

-- Read a sheet other than the first sheet
SELECT * FROM read_gsheet('11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8', sheet='Sheet2');

-- Write a sheet from a table by sheetid
COPY <table_name> TO '11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8' (FORMAT gsheet);

-- Write a sheet from a table by full URL
COPY <table_name> TO 'https://docs.google.com/spreadsheets/d/11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8/edit?usp=sharing' (FORMAT gsheet);
```

## Getting a Google API Access Token

To connect DuckDB to Google Sheets, you’ll need to create a Service Account through the Google API, and use it to generate an access token:

1. Navigate to the [Google API Console](https://console.developers.google.com/apis/library).
2. Create a new project.
3. Search for the Google Sheets API and enable it.
4. In the left-hand navigation, go to the **Credentials** tab.
5. Click **+ Create Credentials** and select **Service Account**.
6. Name the Service Account and assign it the **Owner** role for your project. Click **Done** to save.
7. From the **Service Accounts** page, click on the Service Account you just created.
8. Go to the **Keys** tab, then click **Add Key** > **Create New Key**.
9. Choose **JSON**, then click **Create**. The JSON file will download automatically.
10. Download and install the [gcloud CLI](https://cloud.google.com/sdk/docs/install).
11. Run the following command to login to the gcloud CLI with the Service Account using the newly created JSON file
    ```bash
    gcloud auth activate-service-account --key-file /path/to/key/file
    ```
12. Run the following command to generate an access token:
    ```bash
    gcloud auth print-access-token --scopes=https://www.googleapis.com/auth/spreadsheets
    ```
13. Open your Google Sheet and share it with the Service Account email.
14. Run DuckDB and load the extension

This token will periodically expire - you can re-run the above command again to generate a new one.

## Building
### Managing dependencies
DuckDB extensions uses VCPKG for dependency management. Enabling VCPKG is very simple: follow the [installation instructions](https://vcpkg.io/en/getting-started) or just run the following:
```shell
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_TOOLCHAIN_PATH=`pwd`/vcpkg/scripts/buildsystems/vcpkg.cmake
```
Note: VCPKG is only required for extensions that want to rely on it for dependency management. If you want to develop an extension without dependencies, or want to do your own dependency management, just skip this step. Note that the example extension uses VCPKG to build with a dependency for instructive purposes, so when skipping this step the build may not work without removing the dependency.

### Build steps
Now to build the extension, run:
```sh
make
```
The main binaries that will be built are:
```sh
./build/release/duckdb
./build/release/test/unittest
./build/release/extension/gsheets/gsheets.duckdb_extension
```
- `duckdb` is the binary for the duckdb shell with the extension code automatically loaded.
- `unittest` is the test runner of duckdb. Again, the extension is already linked into the binary.
- `gsheets.duckdb_extension` is the loadable binary as it would be distributed.

## Running the extension
To run the extension code, simply start the shell with `./build/release/duckdb`.

Now we can use the features from the extension directly in DuckDB.

## Running the tests
Different tests can be created for DuckDB extensions. The primary way of testing DuckDB extensions should be the SQL tests in `./test/sql`. These SQL tests can be run using:
```sh
make test
```

### Installing the deployed binaries
To install your extension binaries from S3, you will need to do two things. Firstly, DuckDB should be launched with the
`allow_unsigned_extensions` option set to true. How to set this will depend on the client you're using. Some examples:

CLI:
```shell
duckdb -unsigned
```

Python:
```python
con = duckdb.connect(':memory:', config={'allow_unsigned_extensions' : 'true'})
```

NodeJS:
```js
db = new duckdb.Database(':memory:', {"allow_unsigned_extensions": "true"});
```

Secondly, you will need to set the repository endpoint in DuckDB to the HTTP url of your bucket + version of the extension
you want to install. To do this run the following SQL query in DuckDB:
```sql
SET custom_extension_repository='bucket.s3.eu-west-1.amazonaws.com/<your_extension_name>/latest';
```
Note that the `/latest` path will allow you to install the latest extension version available for your current version of
DuckDB. To specify a specific version, you can pass the version instead.

After running these steps, you can install and load your extension using the regular INSTALL/LOAD commands in DuckDB:
```sql
INSTALL gsheets
LOAD gsheets
```
