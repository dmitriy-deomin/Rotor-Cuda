
//
#include "BinSort.cpp"
#include "Bech32.h"
//-------------------------
#include <iostream>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <iomanip>
#include <array>

//std--------------------------
using std::cout;
using std::string;
using std::vector;
using std::cin;
using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::endl;
using std::cerr;
using std::setlocale;
using std::chrono::system_clock;
using std::isalpha;
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::setw;
using std::setfill;
//---------------------------------------

//цвета---------------------------------------
const char zagolovok[] = "\033[33m";//желтый
const char grey[] = "\033[90m";//серый
const char text_close[] = "\033[37m";//белый
const char text_run_perebor[] = "\033[96m";//голубой
const char white[] = "\033[37m";
//--------------------------------------------

uint64_t counterPROGRESS = 1000000; // через сколько делать обновление прогресса


void create_bat_Brute(const std::string& text, const std::string& bat_file) {
	FILE* file = fopen(bat_file.c_str(), "w");

	if (file == nullptr) {
		// Обработка ошибки открытия файла
		perror("[-] Ошибка при открытии файла");
		return;
	}
	fputs(text.c_str(), file); // Запись строки в файл
	fclose(file); // Закрытие файла
}

void printHelp() {
	cout << "\033[37m"; //белый
	cout << "[+] =====================================================" << endl;
	cout << "[+] .bat ФАЙЛЫ ДЛЯ ЗАПУСКА И КОНВЕРТИРОВАНИЯ СОЗДАННЫ" << endl;
	create_bat_Brute("Rotor-Cuda.exe -g -t 0 --gpui 0 --coin BTC -m addresses --range 80000000000000000:fffffffffffffffff -i btc.bin -n 60 -d 0\npause", "START_BTC.bat");
	create_bat_Brute("::Укажите вместо eth.txt свой список ETH или TRX адресов\nRotor-Cuda.exe -convertFileETH eth.txt\npause", "CONVERT_TXT_ETH_TO_BIN.bat");
	create_bat_Brute("::Укажите вместо btc.txt свой список BTC адресов\nRotor-Cuda.exe -convertFileBTC btc.txt\npause", "CONVERT_TXT_BTC_TO_BIN.bat");
	cout << "[+] =====================================================" << endl << endl;

	cout << "[+] =====================================================" << endl;
	cout << "[+] Список адресов должен быть cконвертирован          " << endl;
	cout << "[+] и сохранен в бинарный файл .bin                    " << endl;
	cout << "[+] конвертировать можно программой через             " << endl;
	cout << "[+] CONVERT_TXT_TO_BIN.bat                            " << endl;
	cout << "[+] =====================================================" << endl << endl;

	cout << "[+] =====================================================" << endl;
	cout << "[+] Список параметров запуска          " << endl;
	cout << "[+] -h вызов родной справки           " << endl;
	cout << "[+] -g поиск с помощью видеокарты          " << endl;
	cout << "[+] --gpui номер видеокарты(обычно 0(--gpui 0)если одна,\n[+] если несколько перечислить через запятую(--gpui 0,1,2)) " << endl;
	cout << "[+] -m режим поиска по:          " << endl;
	cout << "[+]    -m ADDRESS одному адрессу            " << endl;
	cout << "[+]    -m ADDRESSES списку адрессов            " << endl;
	cout << "[+]    -m XPOINT публичному ключу           " << endl;
	cout << "[+]    -m XPOINTS списку ключей            " << endl;
	cout << "[+] --coin BTC/ETH тип адресса          " << endl;
	cout << "[+] --range диапазон поиска          " << endl;
	cout << "[+]   --range 80000000000000000:fffffffffffffffff (68 пазл)       " << endl;
	cout << "[+] -u -поиск только несжатых адрессов          " << endl;
	cout << "[+] -b -поиск сжатых и несжатых адрессов          " << endl;
	cout << "[+] -r включение рандомного диапазона из заданого          " << endl;
	cout << "[+]    -r 1000 (через 1000 милионов смениться)          " << endl;
	cout << "[+] -t количество потоков цпу          " << endl;
	cout << "[+] -o файл куда будет сохраняться найденое          " << endl;
	cout << "[+] -n сохранение контрольной точки (-n 60) -сохранение каждые 60 минут в Rotor-Cuda_Continue.bat " << endl;
	cout << "[+] !!! при новом запуске  Rotor-Cuda_Continue.bat удалять !!!" << endl;
	cout << "[+] -d скрыть отображение лишнего (-d 0) " << endl;
	cout << "[+] =====================================================" << endl << endl;

	cout << "\033[32m" << endl; //зелёный
	cout << "[+] =====================================================" << endl;
	cout << "[+] Программа взята тут" << endl;
	cout << "[+] https://github.com/eric-vader/Rotor-Cuda" << endl << endl;
	cout << "[+] Мои изменения:" << endl;
	cout << "[+] -встроил конвертер" << endl;
	cout << "[+] -изменил вывод" << endl<< endl;
	cout << "[+] Задонатить на чаёк ) " << endl;
	cout << "[+] BTC:bc1qmkd9mxvwqhup3tcs9pwx59lnvd59xptp83c4tt" << endl;
	cout << "[+] ETH:0x129257C352792F398fdb1Df0A79D4608E0893705" << endl;
	cout << "[+] TRX:TBuFpK7NirtgvgMwpPLTtR2C3iFkrwfyj8" << endl;
	cout << "[+] =====================================================" << endl;
}

//что бы цвета работали в консоли
void enableAnsiCodes() {
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE) return;

	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode)) return;

	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hOut, dwMode);
}
//
union hash160_20 {
	uint32_t bits5[5];
	uint8_t bits20[20];
};
//
void readHex(char* buf, const char* txt) {
	char b[3] = "00";
	for (unsigned int i = 0; i < strlen(txt); i += 2) {
		b[0] = *(txt + i);
		b[1] = *(txt + i + 1);
		*(buf + (i >> 1)) = strtoul(b, NULL, 16);
	}
}