<script lang="ts">
	import { page } from '$app/stores';
	import { base } from '$app/paths';
	import { modules, languageTopics } from '$lib/docs-nav';

	let { children } = $props();

	const path = $derived($page.url.pathname);

	// Each nav section is an accordion. On desktop they are expanded by default
	// (and still collapsible); on mobile they start collapsed so the page
	// content is visible immediately without scrolling past a long nav list.
	let languageOpen = $state(true);
	let stdlibOpen = $state(true);

	$effect(() => {
		if (typeof window !== 'undefined' && window.matchMedia('(max-width: 768px)').matches) {
			languageOpen = false;
			stdlibOpen = false;
		}
	});
</script>

<div class="container docs-layout">
	<aside class="docs-sidebar">
		<nav>
			<button
				class="docs-nav-title"
				aria-expanded={languageOpen}
				onclick={() => (languageOpen = !languageOpen)}
			>
				Мова Trypillia
				<span class="docs-nav-chevron">{languageOpen ? '▼' : '▶'}</span>
			</button>
			<div class="docs-nav-group" class:collapsed={!languageOpen}>
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
			</div>

			<button
				class="docs-nav-title"
				aria-expanded={stdlibOpen}
				onclick={() => (stdlibOpen = !stdlibOpen)}
			>
				Стандартна бібліотека
				<span class="docs-nav-chevron">{stdlibOpen ? '▼' : '▶'}</span>
			</button>
			<div class="docs-nav-group" class:collapsed={!stdlibOpen}>
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
			</div>
		</nav>
	</aside>

	<main class="docs-main">
		{@render children()}
	</main>
</div>
