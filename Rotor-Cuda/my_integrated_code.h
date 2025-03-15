
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

//�����---------------------------------------
const char zagolovok[] = "\033[33m";//������
const char grey[] = "\033[90m";//�����
const char text_close[] = "\033[37m";//�����
const char text_run_perebor[] = "\033[96m";//�������
const char white[] = "\033[37m";
//--------------------------------------------

uint64_t counterPROGRESS = 1000000; // ����� ������� ������ ���������� ���������


void create_bat_Brute(const std::string& text, const std::string& bat_file) {
	FILE* file = fopen(bat_file.c_str(), "w");

	if (file == nullptr) {
		// ��������� ������ �������� �����
		perror("[-] ������ ��� �������� �����");
		return;
	}
	fputs(text.c_str(), file); // ������ ������ � ����
	fclose(file); // �������� �����
}

void printHelp() {
	cout << "\033[37m"; //�����
	cout << "[+] =====================================================" << endl;
	cout << "[+] .bat ����� ��� ������� � ��������������� ��������" << endl;
	create_bat_Brute("Rotor-Cuda.exe -g -t 0 --gpui 0 --coin BTC -m addresses --range 80000000000000000:fffffffffffffffff -i btc.bin -n 60 -d 0\npause", "START_BTC.bat");
	create_bat_Brute("::������� ������ eth.txt ���� ������ ETH ��� TRX �������\nRotor-Cuda.exe -convertFileETH eth.txt\npause", "CONVERT_TXT_ETH_TO_BIN.bat");
	create_bat_Brute("::������� ������ btc.txt ���� ������ BTC �������\nRotor-Cuda.exe -convertFileBTC btc.txt\npause", "CONVERT_TXT_BTC_TO_BIN.bat");
	cout << "[+] =====================================================" << endl << endl;

	cout << "[+] =====================================================" << endl;
	cout << "[+] ������ ������� ������ ���� c�������������          " << endl;
	cout << "[+] � �������� � �������� ���� .bin                    " << endl;
	cout << "[+] �������������� ����� ���������� �����             " << endl;
	cout << "[+] CONVERT_TXT_TO_BIN.bat                            " << endl;
	cout << "[+] =====================================================" << endl << endl;

	cout << "[+] =====================================================" << endl;
	cout << "[+] ������ ���������� �������          " << endl;
	cout << "[+] -h ����� ������ �������           " << endl;
	cout << "[+] -g ����� � ������� ����������          " << endl;
	cout << "[+] --gpui ����� ����������(������ 0(--gpui 0)���� ����,\n[+] ���� ��������� ����������� ����� �������(--gpui 0,1,2)) " << endl;
	cout << "[+] -m ����� ������ ��:          " << endl;
	cout << "[+]    -m ADDRESS ������ �������            " << endl;
	cout << "[+]    -m ADDRESSES ������ ��������            " << endl;
	cout << "[+]    -m XPOINT ���������� �����           " << endl;
	cout << "[+]    -m XPOINTS ������ ������            " << endl;
	cout << "[+] --coin BTC/ETH ��� �������          " << endl;
	cout << "[+] --range �������� ������          " << endl;
	cout << "[+]   --range 80000000000000000:fffffffffffffffff (68 ����)       " << endl;
	cout << "[+] -u -����� ������ �������� ��������          " << endl;
	cout << "[+] -b -����� ������ � �������� ��������          " << endl;
	cout << "[+] -r ��������� ���������� ��������� �� ��������          " << endl;
	cout << "[+]    -r 1000 (����� 1000 �������� ���������)          " << endl;
	cout << "[+] -t ���������� ������� ���          " << endl;
	cout << "[+] -o ���� ���� ����� ����������� ��������          " << endl;
	cout << "[+] -n ���������� ����������� ����� (-n 60) -���������� ������ 60 ����� � Rotor-Cuda_Continue.bat " << endl;
	cout << "[+] !!! ��� ����� �������  Rotor-Cuda_Continue.bat ������� !!!" << endl;
	cout << "[+] -d ������ ����������� ������� (-d 0) " << endl;
	cout << "[+] =====================================================" << endl << endl;

	cout << "\033[32m" << endl; //������
	cout << "[+] =====================================================" << endl;
	cout << "[+] ��������� ����� ���" << endl;
	cout << "[+] https://github.com/eric-vader/Rotor-Cuda" << endl << endl;
	cout << "[+] ��� ���������:" << endl;
	cout << "[+] -������� ���������" << endl;
	cout << "[+] -������� �����" << endl<< endl;
	cout << "[+] ���������� �� ��� ) " << endl;
	cout << "[+] BTC:bc1qmkd9mxvwqhup3tcs9pwx59lnvd59xptp83c4tt" << endl;
	cout << "[+] ETH:0x129257C352792F398fdb1Df0A79D4608E0893705" << endl;
	cout << "[+] TRX:TBuFpK7NirtgvgMwpPLTtR2C3iFkrwfyj8" << endl;
	cout << "[+] =====================================================" << endl;
}

//��� �� ����� �������� � �������
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