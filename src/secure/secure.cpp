#include "secure.hpp"          // Заголовочный файл с объявлением функции auth_user
#include <cstring>             // Для функций работы со строками (strdup)
#include <cstdlib>             // Для функций выделения памяти (calloc, free)
#include <security/pam_appl.h> // Основная библиотека PAM в Linux

// Структура для "проброса" пароля внутрь callback-функции PAM
struct pam_auth_data {
    const char* password;      // Храним указатель на строку с паролем
};

// Callback-функция: PAM вызывает её, когда ему что-то нужно от пользователя (например, пароль)
static int pam_conversation(int num_msg, const struct pam_message** msg,
                            struct pam_response** resp, void* appdata_ptr) {
    
    // Приводим указатель appdata_ptr обратно к нашей структуре с паролем
    auto* data = static_cast<pam_auth_data*>(appdata_ptr);
    
    // Если данных нет или пароль пустой, сообщаем PAM об ошибке аутентификации
    if (!data || !data->password) return PAM_AUTH_ERR;

    // Выделяем память под массив ответов (по одному ответу на каждый запрос PAM)
    // Используем calloc, чтобы память была занулена
    auto* responses = static_cast<struct pam_response*>(calloc(num_msg, sizeof(struct pam_response)));
    
    // Если память не выделилась, возвращаем ошибку нехватки памяти
    if (!responses) return PAM_BUF_ERR;

    // Перебираем все сообщения/вопросы от PAM
    for (int i = 0; i < num_msg; ++i) {
        // Проверяем стиль сообщения: PAM_PROMPT_ECHO_OFF значит "запрос пароля" (без эха в консоль)
        // PAM_PROMPT_ECHO_ON — запрос логина или данных, которые можно показывать
        if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF || msg[i]->msg_style == PAM_PROMPT_ECHO_ON) {
            // Копируем пароль в структуру ответа. 
            // ВАЖНО: PAM сам вызовет free() для этой строки, поэтому используем strdup (выделение в куче)
            responses[i].resp = strdup(data->password);
            responses[i].resp_retcode = 0; // Код возврата для этого конкретного ответа (0 - успех)
        } else {
            // Если это просто информационное сообщение от системы, отвечаем NULL
            responses[i].resp = nullptr;
            responses[i].resp_retcode = 0;
        }
    }

    // Записываем указатель на наш массив ответов в выходной параметр resp
    *resp = responses;
    
    // Возвращаем статус успешного завершения "разговора"
    return PAM_SUCCESS;
}

// Главная функция аутентификации
bool auth_user(const std::string& user, const std::string& pass) {
    // Если имя пользователя пустое, сразу выходим
    if (user.empty()) return false;

    // Подготавливаем данные для "разговора" (упаковываем пароль в структуру)
    pam_auth_data data = { pass.c_str() };
    
    // Создаем структуру конфигурации "разговора" (указываем функцию и данные)
    pam_conv conv = { pam_conversation, &data };
    
    // Указатель на обработчик PAM-сессии (будет инициализирован внутри pam_start)
    pam_handle_t* pamh = nullptr;

    // Имя PAM-сервиса (должно совпадать с файлом в /etc/pam.d/)
    const char* pam_service = "login"; 

    // 1. Инициализация PAM: передаем сервис, имя пользователя и структуру разговора
    int retval = pam_start(pam_service, user.c_str(), &conv, &pamh);
    
    // Если инициализация прошла успешно
    if (retval == PAM_SUCCESS) {
        // 2. Выполняем саму проверку подлинности (вызывается наш callback для ввода пароля)
        retval = pam_authenticate(pamh, 0);
    }

    // Если пароль подошел
    if (retval == PAM_SUCCESS) {
        // 3. Проверяем состояние аккаунта (не заблокирован ли, не истек ли срок действия пароля)
        retval = pam_acct_mgmt(pamh, 0);
    }

    // Завершаем работу с PAM, освобождаем ресурсы обработчика pamh
    pam_end(pamh, retval);

    // Если на всех этапах вернулся PAM_SUCCESS, значит пользователь авторизован
    return (retval == PAM_SUCCESS);
}