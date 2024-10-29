---
sidebar_link: false
---

<script>
    let authCode;
    onMount(() => {
        authCode = new URLSearchParams($page.url.hash.substring(1)).get('access_token')
    });
</script>


# DuckDB GSheets

{#if authCode}

## Authorization Successful

Copy the token below and paste it into your DuckDB GSheets application:

<pre><code class="code-box">{authCode}</code></pre>

{:else}


## Authorization Failed

No token was received. Please try the authorization process again. 

You should only need this page when authenticating with Google.

{/if}


<style>
    pre {
        margin: 20px 0;
        padding: 0;
        max-width: 100%;
    }
    .code-box {
        display: block;
        user-select: all;
        cursor: pointer;
        background-color: #f0f0f0;
        padding: 10px;
        border-radius: 5px;
        border: 1px solid #ccc;
        white-space: pre-wrap;
        word-wrap: break-word;
        overflow-wrap: break-word;
        box-sizing: border-box;
        line-height: 1.5;
    }
</style>
