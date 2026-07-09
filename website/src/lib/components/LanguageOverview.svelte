<script lang="ts">
	import { base } from '$app/paths';
	import { languageTopics } from '$lib/docs-nav';
</script>

<svelte:head>
	<title>Мова Trypillia — Документація</title>
</svelte:head>

<nav class="breadcrumb">
	<a href={`${base}/docs`}>Документація</a> / <span>Мова Trypillia</span>
</nav>

<section>
	<h2>Мова Trypillia</h2>
	<p>
		Trypillia — динамічно типізована мова загального призначення зі зрозумілим синтаксисом. Оберіть
		тему, щоб дізнатися більше про базові концепції мови.
	</p>
</section>

<section>
	<div class="pkg-list">
		{#each languageTopics as t (t.slug)}
			<div class="pkg-card">
				<div class="pkg-header">
					<a href={`${base}/docs/language/${t.slug}`} class="pkg-title">{t.title}</a>
				</div>
				<p class="pkg-desc">{t.summary}</p>
			</div>
		{/each}
	</div>
</section>
