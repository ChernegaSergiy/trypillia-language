import { error } from '@sveltejs/kit';
import { modules, getModule } from '$lib/stdlib';

export const prerender = true;

export function entries() {
	return modules.map((m) => ({ module: m.name }));
}

export function load({ params }) {
	const module = getModule(params.module);
	if (!module) {
		throw error(404, `Модуль "${params.module}" не знайдено`);
	}
	return { module };
}
