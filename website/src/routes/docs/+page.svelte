<script lang="ts">
	import { base } from '$app/paths';
	import { modules } from '$lib/docs-nav';
	import DocsBreadcrumb from '$lib/components/DocsBreadcrumb.svelte';

	const total = modules.reduce((sum, m) => sum + m.methods.length, 0);
</script>

<svelte:head>
	<title>Документація — Trypillia</title>
</svelte:head>

<DocsBreadcrumb items={[{ label: 'Документація' }]} />

<section>
	<h2>Документація Trypillia</h2>
	<p>Опис мови програмування та її стандартної бібліотеки.</p>
</section>

<section>
	<h2>Мова програмування</h2>
	<div class="pkg-list">
		<div class="pkg-card">
			<div class="pkg-header">
				<a href={`${base}/docs/language`} class="pkg-title">Синтаксис і базові концепції</a>
			</div>
			<p class="pkg-desc">
				Змінні, функції, класи, керуючі конструкції та інші базові елементи мови Trypillia.
			</p>
		</div>
	</div>
</section>

<section>
	<h2>Стандартна бібліотека</h2>
	<p>{modules.length} модулів, {total} задокументованих методів.</p>
	<div class="pkg-list">
		{#each modules as m (m.slug)}
			<div class="pkg-card">
				<div class="pkg-header">
					<a href={`${base}/docs/stdlib/${m.slug}`} class="pkg-title">{m.title}</a>
					<code>{m.methods.length}</code>
				</div>
				<p class="pkg-desc">{m.description}</p>
			</div>
		{/each}
	</div>
</section>
