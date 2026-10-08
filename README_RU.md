# Клиент PowVPN

[PowVPN](https://powvpn.com) — сервис для защищённого доступа в интернет с приложениями для Windows, Linux и Android. Клиент поддерживает подключение к локациям PowVPN через AmneziaWG 3.1, WireGuard, VLESS Reality и IKEv2/IPsec, если соответствующий протокол доступен на выбранной локации.

Этот репозиторий содержит открытый исходный код клиента PowVPN. Проект является модифицированной версией [клиента Amnezia VPN](https://github.com/amnezia-vpn/amnezia-client) с собственным оформлением PowVPN, авторизацией, управляемым выбором серверов и интеграцией с браузерными расширениями.

## Основные изменения PowVPN

- Брендинг, идентификаторы приложения, иконки и параметры установщиков PowVPN.
- Авторизация через аккаунт PowVPN и регистрация устройства.
- Автоматический выбор сервера или выбор страны из сети PowVPN.
- Управляемое подключение с резервными протоколами.
- Native Messaging bridge для расширений PowVPN.
- Интерфейс, переводы и сборка релизов PowVPN.

## Исходный проект

PowVPN Client создан на основе `amnezia-vpn/amnezia-client` с сохранением истории Git, уведомлений об авторских правах и лицензии GNU GPL v3. Начальный набор модификаций PowVPN основан на upstream-коммите [`94b51df24790bf52427afe82d81c87a95460bdfd`](https://github.com/amnezia-vpn/amnezia-client/commit/94b51df24790bf52427afe82d81c87a95460bdfd).

PowVPN является независимым проектом и не связан с командой Amnezia VPN. Дополнительная информация приведена в [NOTICE.md](NOTICE.md), а лицензии сторонних компонентов — в [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

## Сборка

Клонируйте репозиторий вместе с подмодулями:

```bash
git clone --recurse-submodules https://github.com/powvpn/powvpn-client.git
cd powvpn-client
```

Для сборки используются CMake, Qt 6 и Conan. Локальные настройки, результаты сборки, ключи подписи, keystore-файлы и сгенерированные файлы зависимостей не должны добавляться в Git.

Сборка Windows запускается из настроенного окружения Qt/MSVC:

```powershell
.\deploy\build.bat
```

## Ссылки

- Сайт: [https://powvpn.com](https://powvpn.com)
- Скачать: [https://powvpn.com/download](https://powvpn.com/download)
- Политика конфиденциальности: [https://powvpn.com/privacy-policy](https://powvpn.com/privacy-policy)

## Безопасность

Не публикуйте уязвимости и конфиденциальные данные в открытых Issues. Сообщения о безопасности можно отправить на `info@powvpn.com`.

## Лицензия

Исходный код распространяется по лицензии [GNU General Public License v3.0](LICENSE), как и исходный проект Amnezia VPN. Сторонние компоненты используются на условиях их собственных лицензий, перечисленных в [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

Название и логотипы PowVPN обозначают сервис PowVPN. GPL применяется к исходному коду и не предоставляет прав на товарные знаки.
