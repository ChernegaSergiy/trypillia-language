<script lang="ts">
	import { base } from '$app/paths';

	let {
		module,
		title,
		name
	}: { module: string; title: string; name: string } = $props();
</script>

<nav class="breadcrumb">
	<a href={`${base}/docs/stdlib`}>Стандартна бібліотека</a> /
	<a href={`${base}/docs/stdlib/${module}`}>{title}</a> /
	<span>{name}</span>
</nav>
