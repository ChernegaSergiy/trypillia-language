<script lang="ts">
	import { page } from '$app/stores';
	import { base } from '$app/paths';
	import { modules, languageTopics } from '$lib/docs-nav';

	let { children } = $props();

	const path = $derived($page.url.pathname);

	// On mobile the sidebar is collapsed by default so the page content is
	// visible immediately without scrolling past a long nav list. The desktop
	// layout (the default) always shows the nav and ignores this flag.
	let navOpen = $state(false);

	// Collapse the mobile nav after navigating to a new page.
	$effect(() => {
		path;
		navOpen = false;
	});
</script>

<div class="container docs-layout">
	<aside class="docs-sidebar">
		<button
			class="sidebar-toggle"
			aria-expanded={navOpen}
			onclick={() => (navOpen = !navOpen)}
		>
			Навігація <span class="sidebar-toggle-chevron">{navOpen ? '▲' : '▼'}</span>
		</button>
		<nav class:collapsed={!navOpen}>
			<p class="docs-nav-title">Мова Trypillia</p>
			<a href={`${base}/docs/language`} class:active={path === '/docs/language'}>Огляд</a>
			<ul class="docs-module-list">
				{#each languageTopics as t (t.slug)}
					<li>
						<a
							href={`${base}/docs/language/${t.slug}`}
							class:active={path === `/docs/language/${t.slug}`}
						>
							{t.title}
						</a>
					</li>
				{/each}
			</ul>

			<p class="docs-nav-title">Стандартна бібліотека</p>
			<ul class="docs-module-list">
				{#each modules as m (m.slug)}
					<li>
						<a
							href={`${base}/docs/stdlib/${m.slug}`}
							class:active={path.startsWith(`/docs/stdlib/${m.slug}`)}
						>
							{m.title}
							<span class="docs-module-count">{m.methods.length}</span>
						</a>
					</li>
				{/each}
			</ul>
		</nav>
	</aside>

	<main class="docs-main">
		{@render children()}
	</main>
</div>
