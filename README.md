# DevSpace

[![C++](https://img.shields.io/badge/Language-C%2B%2B-blue.svg)](https://isocpp.org/)
[![Framework](https://img.shields.io/badge/Backend-Crow-green.svg)](https://crowcpp.org/)
[![Auth](https://img.shields.io/badge/Auth-PAM-red.svg)](https://www.linux-pam.org/)
[![Editor](https://img.shields.io/badge/Editor-Monaco-orange.svg)](https://microsoft.github.io/monaco-editor/)

**DevSpace** — это профессиональная веб-платформа для превращения Linux-хоста (PC, Raspberry Pi, Server) в интегрированную среду разработки и управления проектами. Разработано специально для **ElyOS**.

## 🚀 Технологическая база
- **Backend:** C++ с использованием микрофреймворка **Crow**. Высокая производительность и низкий overhead.
- **Security:** Нативная аутентификация через **PAM**. Прямая интеграция с системными пользователями Linux.
- **Frontend IDE:** **Monaco Editor**. Полноценный опыт VS Code прямо в браузере (рендеринг на стороне клиента).
- **Database:** SQLite для эффективного хранения метаданных проектов и реестра путей.
- **Terminal:** Встроенная веб-консоль для прямого взаимодействия с оболочкой (Shell) через браузер.

## ✨ Основные функции
- **Project Tiling UI:** Интерфейс в стиле тайлинговых оконных менеджеров для максимальной концентрации.
- **FS Manager:** Проводник с логикой классических FM (навигация, операции с файлами).
- **Project Lifecycle:** Автоматическая инициализация (Git, README, иконки).
- **Integrity Monitor:** SQLite-реестр отслеживает состояние проектов. Если папка удалена из терминала — система предложит протокол очистки или восстановления.
- **Web Console:** Полноценный доступ к Bash/Zsh/Fish без необходимости SSH-клиента.