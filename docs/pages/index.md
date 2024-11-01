---
title: DuckDB for Google Sheets
hide_title: true
---

<h1 class="markdown flex items-center gap-2"><img src="icon-512.png" style="height: 1em;"/> DuckDB for Google Sheets</h1>

<Alert status="warning">

**🚧 WARNING - Work in Progress 🚧**

Here be many dragons 
</Alert>


A DuckDB extension that allows you to read and write Google Sheets using SQL.

Note: This project is not affliated with Google or DuckDB.

## Install

```sql
INSTALL gsheets FROM community;
LOAD gsheets;
```

The latest version of DuckDB (currently 1.1.2) is required.

## Usage 

### Authenticate

```sql
-- Authenticate with Google Account in the browser (easiest)
CREATE SECRET (TYPE gsheet, PROVIDER oauth);

-- OR create a secret with your Google API access token (boring, see below guide)
CREATE SECRET (TYPE gsheet, TOKEN '<your_token>');
```

### Read

```sql
-- Read a spreadsheet by full URL
FROM read_gsheet('https://docs.google.com/spreadsheets/d/11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8/edit');

-- Read a spreadsheet by full URL, implicitly
FROM 'https://docs.google.com/spreadsheets/d/11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8/edit';

-- Read a spreadsheet by spreadsheet id
FROM read_gsheet('11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8');

-- Read a spreadsheet with no header row
SELECT * FROM read_gsheet('11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8', headers=false);

-- Read a sheet other than the first sheet using the sheet name
SELECT * FROM read_gsheet('11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8', sheet='Sheet2');

-- Read a sheet other than the first sheet using the sheet id in the URL
SELECT * FROM read_gsheet('https://docs.google.com/spreadsheets/d/11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8/edit?gid=644613997#gid=644613997');
```

### Write

```sql
-- Write a spreadsheet from a table by spreadsheet id
COPY <table_name> TO '11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8' (FORMAT gsheet);

-- Write a spreadsheet from a table by full URL
COPY <table_name> TO 'https://docs.google.com/spreadsheets/d/11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8/edit?usp=sharing' (FORMAT gsheet);

-- Write a spreadsheet to a specific sheet using the sheet id in the URL
COPY <table_name> TO 'https://docs.google.com/spreadsheets/d/11QdEasMWbETbFVxry-SsD8jVcdYIT1zBQszcF84MdE8/edit?gid=1295634987#gid=1295634987' (FORMAT gsheet);
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

## Limitations / Known Issues

- Google Sheets has a limit of 1,000,000 cells per spreadsheet.
- The OAuth app has not yet been approved by Google, so will throw a warning - you must select "Contine (Unsafe)" to use it.
- Reading sheets where data does not start in A1 is not yet supported.
- Writing data to a sheet starting from a cell other than A1 is not yet supported.
- Sheets must already exist to COPY TO them.
