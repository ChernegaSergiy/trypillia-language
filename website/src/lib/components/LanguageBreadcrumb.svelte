<script lang="ts">
	import DocsBreadcrumb from '$lib/components/DocsBreadcrumb.svelte';

	let { title }: { title: string } = $props();
</script>

<DocsBreadcrumb
	items={[
		{ href: '/docs', label: 'Документація' },
		{ href: '/docs/language', label: 'Мова Trypillia' },
		{ label: title }
	]}
/>
