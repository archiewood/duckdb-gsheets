---
title: Privacy Policy
---

**Effective Date: 2024-10-24**

This policy outlines the practices regarding the collection, use, and security of your personal data when using DuckDB GSheets. *DuckDB GSheets* is designed to query and interact with Google Sheets from your local DuckDB environment. 

## 1. Personal Data Collection

*DuckDB GSheets* does not collect, store, or share any personal data with third parties. All operations, including queries and data manipulations, are executed on your local machine. No data is transmitted to any external servers controlled by us.

## 2. OAuth Authentication and Scopes

If you choose to use the OAuth authentication, the application requires access to your Google Sheets through OAuth authentication using the following scope:
- `https://www.googleapis.com/auth/spreadsheets`

This scope allows the app to read and modify your Google Sheets data. The OAuth flow uses Proof Key for Code Exchange (PKCE) for enhanced security. All authentication and authorization processes occur securely between your local system and Google’s OAuth service.

## 3. Token Management

OAuth tokens are stored locally on your machine using DuckDB's secret manager. These tokens are never transmitted to us or any third-party services. You have full control over your authentication tokens, and they remain securely stored on your device.

## 4. Data Storage

Any data accessed from Google Sheets is only stored locally on your machine if you explicitly instruct the DuckDB application to do so. For instance, if you:
- Use the `CREATE TABLE` functionality in DuckDB,
- Export data to a CSV file,

then the resulting data is stored on your local system. Otherwise, no data is retained after the DuckDB session ends.

## 5. Third-Party Services

*DuckDB GSheets* does not interact with any third-party services, aside from connecting to the Google Sheets API to facilitate querying your spreadsheets. We do not use any analytics or data collection services.

## 6. Security

The application uses industry-standard security practices, including the OAuth flow with PKCE (Proof Key for Code Exchange) for secure authentication. All data is handled locally, and no information is transmitted externally unless initiated by the user (e.g., exporting data).

## 7. Your Control Over Data

Since all data and operations are managed locally on your device, you retain full control over the use, storage, and deletion of any data accessed via *DuckDB GSheets*. You may delete any locally stored data at any time by removing the relevant files from your machine.

## 8. Changes to This Privacy Policy

We may update this privacy policy from time to time. Any changes will be reflected with an updated effective date. Continued use of the application following changes constitutes your acceptance of those changes.

## 9. Contact Us

If you have any questions or concerns about this privacy policy, please contact archieemwood@gmail.com.
