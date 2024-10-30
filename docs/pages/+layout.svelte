<script>
	import '@evidence-dev/tailwind/fonts.css';
	import '../app.css';
	import { EvidenceDefaultLayout } from '@evidence-dev/core-components';
	export let data;
</script>

<EvidenceDefaultLayout {data} hideBreadcrumbs logo="/duckdb-gsheets.png" githubRepo="https://github.com/evidence-dev/duckdb_gsheets">
	<slot slot="content" />
</EvidenceDefaultLayout>
