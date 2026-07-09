// Hand-written navigation index for the standard-library documentation.
// This file only describes the STRUCTURE (which modules and methods exist and a
// one-line summary for cards/sidebar). The full documentation for every method
// is written by hand in the corresponding `+page.svx` route file.

export interface MethodEntry {
	/** Method name, also used as the URL slug. */
	name: string;
	/** Display label (e.g. with parentheses) shown in cards. */
	label: string;
	/** One-line Ukrainian summary shown in listings. */
	summary: string;
}

export interface ModuleEntry {
	/** Module name, also used as the URL slug. */
	slug: string;
	/** Display title in the sidebar and headings. */
	title: string;
	/** Short Ukrainian description of the module. */
	description: string;
	methods: MethodEntry[];
}

export const modules: ModuleEntry[] = [
	{
		slug: 'Core',
		title: 'Core',
		description: 'Глобальні функції, доступні без імпорту.',
		methods: [
			{ name: 'print', label: 'print()', summary: 'Виводить значення у стандартний вивід.' },
			{ name: 'input', label: 'input()', summary: 'Зчитує рядок, введений користувачем.' }
		]
	},
	{
		slug: 'Error',
		title: 'Error',
		description: 'Клас помилки з повідомленням та кодом.',
		methods: [
			{ name: 'init', label: 'init()', summary: 'Створює обʼєкт помилки.' }
		]
	},
	{
		slug: 'Result',
		title: 'Result',
		description: 'Тип результату для явної обробки успіху та помилок.',
		methods: [
			{ name: 'ok', label: 'ok()', summary: 'Створює успішний результат.' },
			{ name: 'err', label: 'err()', summary: 'Створює результат-помилку.' },
			{ name: 'isOk', label: 'isOk()', summary: 'Перевіряє, чи результат успішний.' },
			{ name: 'isErr', label: 'isErr()', summary: 'Перевіряє, чи результат є помилкою.' },
			{ name: 'unwrap', label: 'unwrap()', summary: 'Повертає значення або аварійно завершує.' },
			{ name: 'unwrapErr', label: 'unwrapErr()', summary: 'Повертає помилку або аварійно завершує.' }
		]
	},
	{
		slug: 'WeakRef',
		title: 'WeakRef',
		description: 'Слабке посилання, що не заважає збиранню сміття.',
		methods: [
			{ name: 'init', label: 'init()', summary: 'Створює слабке посилання на обʼєкт.' },
			{ name: 'lock', label: 'lock()', summary: 'Повертає обʼєкт або null, якщо його вже зібрано.' }
		]
	},
	{
		slug: 'Math',
		title: 'Math',
		description: 'Математичні функції та константи.',
		methods: [
			{ name: 'sin', label: 'sin()', summary: 'Синус кута в радіанах.' },
			{ name: 'cos', label: 'cos()', summary: 'Косинус кута в радіанах.' },
			{ name: 'random', label: 'random()', summary: 'Випадкове число від 0 до 1.' },
			{ name: 'PI', label: 'PI', summary: 'Константа числа Пі.' }
		]
	},
	{
		slug: 'String',
		title: 'String',
		description: 'Операції над рядками.',
		methods: [
			{ name: 'length', label: 'length()', summary: 'Довжина рядка.' },
			{ name: 'substring', label: 'substring()', summary: 'Підрядок за початком і довжиною.' },
			{ name: 'toUpper', label: 'toUpper()', summary: 'Переводить рядок у верхній регістр.' },
			{ name: 'toLower', label: 'toLower()', summary: 'Переводить рядок у нижній регістр.' },
			{ name: 'trim', label: 'trim()', summary: 'Видаляє пробіли з країв.' },
			{ name: 'split', label: 'split()', summary: 'Розбиває рядок на список за роздільником.' },
			{ name: 'replace', label: 'replace()', summary: 'Замінює всі входження підрядка.' },
			{ name: 'indexOf', label: 'indexOf()', summary: 'Індекс першого входження підрядка.' },
			{ name: 'includes', label: 'includes()', summary: 'Чи містить рядок підрядок.' },
			{ name: 'startsWith', label: 'startsWith()', summary: 'Чи починається рядок із префікса.' },
			{ name: 'endsWith', label: 'endsWith()', summary: 'Чи закінчується рядок суфіксом.' },
			{ name: 'toNumber', label: 'toNumber()', summary: 'Перетворює рядок на число.' }
		]
	},
	{
		slug: 'List',
		title: 'List',
		description: 'Операції над списками (масивами) та ітерація.',
		methods: [
			{ name: 'length', label: 'length()', summary: 'Кількість елементів у списку.' },
			{ name: 'push', label: 'push()', summary: 'Додає елемент у кінець списку.' },
			{ name: 'pop', label: 'pop()', summary: 'Видаляє та повертає останній елемент.' },
			{ name: 'insert', label: 'insert()', summary: 'Вставляє елемент за індексом.' },
			{ name: 'remove', label: 'remove()', summary: 'Видаляє елемент за індексом.' },
			{ name: 'reverse', label: 'reverse()', summary: 'Обертає порядок елементів.' },
			{ name: 'join', label: 'join()', summary: 'Обʼєднує елементи в рядок.' },
			{ name: 'map', label: 'map()', summary: 'Створює новий список, застосувавши функцію.' },
			{ name: 'filter', label: 'filter()', summary: 'Фільтрує елементи за предикатом.' },
			{ name: 'forEach', label: 'forEach()', summary: 'Виконує функцію для кожного елемента.' }
		]
	},
	{
		slug: 'Map',
		title: 'Map',
		description: 'Операції над асоціативними масивами (словниками).',
		methods: [
			{ name: 'keys', label: 'keys()', summary: 'Список ключів.' },
			{ name: 'values', label: 'values()', summary: 'Список значень.' },
			{ name: 'has', label: 'has()', summary: 'Чи існує ключ.' },
			{ name: 'remove', label: 'remove()', summary: 'Видаляє пару за ключем.' },
			{ name: 'set', label: 'set()', summary: 'Встановлює значення за ключем.' }
		]
	},
	{
		slug: 'Time',
		title: 'Time',
		description: 'Робота з часом і затримками.',
		methods: [
			{ name: 'now', label: 'now()', summary: 'Поточний час у секундах Unix.' },
			{ name: 'sleep', label: 'sleep()', summary: 'Призупиняє виконання на задану кількість мілісекунд.' },
			{ name: 'format', label: 'format()', summary: 'Форматує мітку часу в рядок.' }
		]
	},
	{
		slug: 'Random',
		title: 'Random',
		description: 'Генерація випадкових значень.',
		methods: [
			{ name: 'int', label: 'int()', summary: 'Випадкове ціле в діапазоні.' },
			{ name: 'float', label: 'float()', summary: 'Випадкове дробове в діапазоні.' },
			{ name: 'choice', label: 'choice()', summary: 'Випадковий елемент зі списку.' },
			{ name: 'uuid', label: 'uuid()', summary: 'Генерує UUID версії 4.' }
		]
	},
	{
		slug: 'OS',
		title: 'OS',
		description: 'Взаємодія з операційною системою.',
		methods: [
			{ name: 'getEnv', label: 'getEnv()', summary: 'Значення змінної середовища.' },
			{ name: 'cwd', label: 'cwd()', summary: 'Поточний робочий каталог.' },
			{ name: 'exec', label: 'exec()', summary: 'Виконує команду оболонки.' },
			{ name: 'exit', label: 'exit()', summary: 'Завершує програму з кодом.' },
			{ name: 'args', label: 'args()', summary: 'Аргументи командного рядка.' }
		]
	},
	{
		slug: 'Terminal',
		title: 'Terminal',
		description: 'Керування терміналом і курсором.',
		methods: [
			{ name: 'enableRawMode', label: 'enableRawMode()', summary: 'Вмикає сирий режим вводу.' },
			{ name: 'disableRawMode', label: 'disableRawMode()', summary: 'Вимикає сирий режим вводу.' },
			{ name: 'readChar', label: 'readChar()', summary: 'Зчитує один символ.' },
			{ name: 'write', label: 'write()', summary: 'Виводить текст без переносу рядка.' },
			{ name: 'color', label: 'color()', summary: 'Встановлює колір тексту.' },
			{ name: 'reset', label: 'reset()', summary: 'Скидає форматування.' },
			{ name: 'clear', label: 'clear()', summary: 'Очищає екран.' },
			{ name: 'setCursor', label: 'setCursor()', summary: 'Переміщає курсор.' },
			{ name: 'getCursor', label: 'getCursor()', summary: 'Повертає позицію курсора.' }
		]
	},
	{
		slug: 'Regex',
		title: 'Regex',
		description: 'Регулярні вирази.',
		methods: [
			{ name: 'test', label: 'test()', summary: 'Перевіряє відповідність шаблону.' },
			{ name: 'match', label: 'match()', summary: 'Повертає збіги та групи.' },
			{ name: 'replace', label: 'replace()', summary: 'Замінює збіги за шаблоном.' }
		]
	},
	{
		slug: 'Json',
		title: 'Json',
		description: 'Серіалізація та розбір JSON.',
		methods: [
			{ name: 'stringify', label: 'stringify()', summary: 'Перетворює значення на рядок JSON.' },
			{ name: 'parse', label: 'parse()', summary: 'Розбирає рядок JSON у значення.' }
		]
	},
	{
		slug: 'File',
		title: 'File',
		description: 'Робота з файловою системою.',
		methods: [
			{ name: 'open', label: 'open()', summary: 'Відкриває файл і повертає обʼєкт файлу.' },
			{ name: 'exists', label: 'exists()', summary: 'Чи існує файл.' },
			{ name: 'remove', label: 'remove()', summary: 'Видаляє файл.' },
			{ name: 'read', label: 'read()', summary: 'Читає вміст файлу.' },
			{ name: 'write', label: 'write()', summary: 'Записує вміст у файл.' },
			{ name: 'close', label: 'close()', summary: 'Закриває файл.' }
		]
	},
	{
		slug: 'Http',
		title: 'Http',
		description: 'HTTP/HTTPS-запити.',
		methods: [
			{ name: 'get', label: 'get()', summary: 'Виконує GET-запит.' },
			{ name: 'post', label: 'post()', summary: 'Виконує POST-запит.' }
		]
	},
	{
		slug: 'Socket',
		title: 'Socket',
		description: 'TCP-зʼєднання (клієнт і сервер).',
		methods: [
			{ name: 'connect', label: 'connect()', summary: 'Підключається до хоста та порту.' },
			{ name: 'listen', label: 'listen()', summary: 'Слухає вхідні зʼєднання на порту.' },
			{ name: 'accept', label: 'accept()', summary: 'Приймає вхідне зʼєднання.' },
			{ name: 'send', label: 'send()', summary: 'Надсилає дані.' },
			{ name: 'recv', label: 'recv()', summary: 'Отримує дані.' },
			{ name: 'close', label: 'close()', summary: 'Закриває зʼєднання.' }
		]
	},
	{
		slug: 'WebSocketServer',
		title: 'WebSocketServer',
		description: 'Сервер WebSocket.',
		methods: [
			{ name: 'create', label: 'create()', summary: 'Створює сервер на порту.' },
			{ name: 'accept', label: 'accept()', summary: 'Приймає клієнтське зʼєднання.' }
		]
	},
	{
		slug: 'WebSocket',
		title: 'WebSocket',
		description: 'Клієнтське зʼєднання WebSocket.',
		methods: [
			{ name: 'send', label: 'send()', summary: 'Надсилає повідомлення.' },
			{ name: 'recv', label: 'recv()', summary: 'Отримує повідомлення.' },
			{ name: 'close', label: 'close()', summary: 'Закриває зʼєднання.' }
		]
	},
	{
		slug: 'Worker',
		title: 'Worker',
		description: 'Паралельне виконання скриптів у потоках.',
		methods: [
			{ name: 'create', label: 'create()', summary: 'Запускає скрипт у новому потоці.' },
			{ name: 'send', label: 'send()', summary: 'Надсилає повідомлення воркеру.' },
			{ name: 'receive', label: 'receive()', summary: 'Отримує повідомлення від воркера.' },
			{ name: 'selfSend', label: 'selfSend()', summary: 'Надсилає повідомлення з воркера в головний потік.' },
			{ name: 'selfReceive', label: 'selfReceive()', summary: 'Отримує повідомлення в воркері з головного потоку.' }
		]
	},
	{
		slug: 'Crypto',
		title: 'Crypto',
		description: 'Хешування та кодування.',
		methods: [
			{ name: 'sha1', label: 'sha1()', summary: 'Хеш SHA-1 у шістнадцятковому вигляді.' },
			{ name: 'sha1Base64', label: 'sha1Base64()', summary: 'Хеш SHA-1 у кодуванні Base64.' },
			{ name: 'base64Encode', label: 'base64Encode()', summary: 'Кодує рядок у Base64.' }
		]
	},
	{
		slug: 'Test',
		title: 'Test',
		description: 'Глобальні функції для написання тестів.',
		methods: [
			{ name: 'assert', label: 'assert()', summary: 'Перевіряє, що умова істинна.' },
			{ name: 'assertEq', label: 'assertEq()', summary: 'Перевіряє рівність значень.' },
			{ name: 'assertNeq', label: 'assertNeq()', summary: 'Перевіряє нерівність значень.' },
			{ name: 'assertThrows', label: 'assertThrows()', summary: 'Перевіряє, що функція кидає помилку.' },
			{ name: 'describe', label: 'describe()', summary: 'Групує тести.' },
			{ name: 'it', label: 'it()', summary: 'Оголошує тест-кейс.' },
			{ name: 'xit', label: 'xit()', summary: 'Пропускає тест-кейс.' },
			{ name: 'fit', label: 'fit()', summary: 'Фокусує окремий тест-кейс.' },
			{ name: 'fdescribe', label: 'fdescribe()', summary: 'Фокусує групу тестів.' },
			{ name: 'before', label: 'before()', summary: 'Виконується перед усіма тестами.' },
			{ name: 'after', label: 'after()', summary: 'Виконується після всіх тестів.' },
			{ name: 'beforeEach', label: 'beforeEach()', summary: 'Виконується перед кожним тестом.' },
			{ name: 'afterEach', label: 'afterEach()', summary: 'Виконується після кожного тесту.' }
		]
	},
	{
		slug: 'Promise',
		title: 'Promise',
		description: 'Асинхронні обчислення та ланцюжки обробки.',
		methods: [
			{ name: 'Promise', label: 'Promise()', summary: 'Створює проміс із виконавцем.' },
			{ name: 'then', label: 'then()', summary: 'Додає обробники результату.' }
		]
	}
];

export function getModule(slug: string): ModuleEntry | undefined {
	return modules.find((m) => m.slug === slug);
}

// --- Language guide topics ---

export interface TopicEntry {
	/** Topic slug, also used as the URL segment. */
	slug: string;
	/** Display title in the sidebar and headings. */
	title: string;
	/** One-line Ukrainian summary shown in listings. */
	summary: string;
}

export const languageTopics: TopicEntry[] = [
	{
		slug: 'variables',
		title: 'Змінні та константи',
		summary: 'Оголошення, присвоєння, область видимості та незмінні значення.'
	},
	{
		slug: 'types',
		title: 'Типи даних',
		summary: 'Числа, рядки, булеві значення, nil, списки та словники.'
	},
	{
		slug: 'strings',
		title: 'Рядки',
		summary: 'Інтерполяція, екранування та індексація рядків.'
	},
	{
		slug: 'operators',
		title: 'Оператори',
		summary: 'Арифметика, порівняння, логіка, побітові оператори та присвоєння.'
	},
	{
		slug: 'control-flow',
		title: 'Керуючі конструкції',
		summary: 'Умови, цикли та розгалуження switch.'
	},
	{
		slug: 'functions',
		title: 'Функції',
		summary: 'Оголошення, анонімні функції, замикання та функції як значення.'
	},
	{
		slug: 'classes',
		title: 'Класи',
		summary: 'Поля, конструктор, методи, модифікатори доступу та static.'
	},
	{
		slug: 'inheritance',
		title: 'Наслідування',
		summary: 'Успадкування, super, абстрактні класи, інтерфейси та трейти.'
	}
];

export function getTopic(slug: string): TopicEntry | undefined {
	return languageTopics.find((t) => t.slug === slug);
}
