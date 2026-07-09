<script lang="ts">
	import { base } from '$app/paths';

	let { title }: { title: string } = $props();
</script>

<nav class="breadcrumb">
	<a href={`${base}/docs`}>Документація</a> /
	<a href={`${base}/docs/language`}>Мова Trypillia</a> /
	<span>{title}</span>
</nav>
