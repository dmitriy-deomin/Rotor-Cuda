#include "Timer.h"
#include "Rotor.h"
#include "Base58.h"
#include "CmdParse.h"
#include <fstream>
#include <string>
#include <string.h>
#include <stdexcept>
#include <cassert>
#include <algorithm>
#ifndef WIN64
#include <signal.h>
#include <unistd.h>
#endif

//
#include "my_integrated_code.h"
//

#define RELEASE "1.07                         (15.03.2025)"

using namespace std;
bool should_exit = false;

// ----------------------------------------------------------------------------
void usage()
{
	printf("Rotor-Cuda [OPTIONS...] [TARGETS]\n");
	printf("Where TARGETS is one address/xpont, or multiple hashes/xpoints file\n\n");

	printf("-h, --help                               : Display this message\n");
	printf("-c, --check                              : Check the working of the codes\n");
	printf("-u, --uncomp                             : Search uncompressed points\n");
	printf("-b, --both                               : Search both uncompressed or compressed points\n");
	printf("-g, --gpu                                : Enable GPU calculation\n");
	printf("--gpui GPU ids: 0,1,...                  : List of GPU(s) to use, default is 0\n");
	printf("--gpux GPU gridsize: g0x,g0y,g1x,g1y,... : Specify GPU(s) kernel gridsize, default is 8*(Device MP count),128\n");
	printf("-t, --thread N                           : Specify number of CPU thread, default is number of core\n");
	printf("-i, --in FILE                            : Read rmd160 hashes or xpoints from FILE, should be in binary format with sorted\n");
	printf("-o, --out FILE                           : Write keys to FILE, default: Found.txt\n");
	printf("-m, --mode MODE                          : Specify search mode where MODE is\n");
	printf("                                               ADDRESS  : for single address\n");
	printf("                                               ADDRESSES: for multiple hashes/addresses\n");
	printf("                                               XPOINT   : for single xpoint\n");
	printf("                                               XPOINTS  : for multiple xpoints\n");
	printf("--coin BTC/ETH                           : Specify Coin name to search\n");
	printf("                                               BTC: available mode :-\n");
	printf("                                                   ADDRESS, ADDRESSES, XPOINT, XPOINTS\n");
	printf("                                               ETH: available mode :-\n");
	printf("                                                   ADDRESS, ADDRESSES\n");
	printf("-l, --list                               : List cuda enabled devices\n");
	printf("--range KEYSPACE                         : Specify the range:\n");
	printf("                                               START:END\n");
	printf("                                               START:+COUNT\n");
	printf("                                               START\n");
	printf("                                               :END\n");
	printf("                                               :+COUNT\n");
	printf("                                               Where START, END, COUNT are in hex format\n");
	printf("-r, --rkey Rkey                          : Reloads random start Private key every (-r 10 = 10.000.000.000), default is disabled\n");
	printf("-c, --check                              : Check the working of the codes\n");
	printf("-n, --next                               : Range mode GPU save checkpoints every -n ? minutes (1-10000) Default: false\n");
	printf("-n, --next                               : Random mode: What bit range to randomize? -n 1-256 Example -n 252 Default: 252-256 bit\n");
	printf("-z, --zet                                : Random mode: End range for random. Example -n 48 -z 52 (random in 48,49,50,51,52 bit) Default: false\n");
	printf("-d, --display                            : Disable all informers, compact version for many GPUs. Use -d 0 Default -d 1 (display all) \n");
	printf("-convertFileETH                          : Сonverting a list of eth addresses to a bin file (-convertFileETH eth.txt) \n");
	printf("-convertFileBTC                          : Сonverting a list of btc addresses to a bin file (-convertFileBTC btc.txt) \n");
}

// ----------------------------------------------------------------------------

void getInts(string name, vector<int>& tokens, const string& text, char sep)
{

	size_t start = 0, end = 0;
	tokens.clear();
	int item;

	try {

		while ((end = text.find(sep, start)) != string::npos) {
			item = std::stoi(text.substr(start, end - start));
			tokens.push_back(item);
			start = end + 1;
		}

		item = std::stoi(text.substr(start));
		tokens.push_back(item);

	}
	catch (std::invalid_argument&) {

		printf("  Invalid %s argument, number expected\n", name.c_str());
		usage();
		exit(-1);

	}

}

// ----------------------------------------------------------------------------

int parseSearchMode(const std::string& s)
{
	std::string stype = s;
	std::transform(stype.begin(), stype.end(), stype.begin(), ::tolower);

	if (stype == "address") {
		return SEARCH_MODE_SA;
	}

	if (stype == "xpoint") {
		return SEARCH_MODE_SX;
	}

	if (stype == "addresses") {
		return SEARCH_MODE_MA;
	}

	if (stype == "xpoints") {
		return SEARCH_MODE_MX;
	}

	printf("  Invalid search mode format: %s", stype.c_str());
	usage();
	exit(-1);
}

// ----------------------------------------------------------------------------

int parseCoinType(const std::string& s)
{
	std::string stype = s;
	std::transform(stype.begin(), stype.end(), stype.begin(), ::tolower);

	if (stype == "btc") {
		return COIN_BTC;
	}

	if (stype == "eth") {
		return COIN_ETH;
	}

	printf("  Invalid coin name: %s", stype.c_str());
	usage();
	exit(-1);
}

// ----------------------------------------------------------------------------

bool parseRange(const std::string& s, Int& start, Int& end)
{
	size_t pos = s.find(':');

	if (pos == std::string::npos) {
		start.SetBase16(s.c_str());
		end.Set(&start);
		end.Add(0xFFFFFFFFFFFFULL);
	}
	else {
		std::string left = s.substr(0, pos);

		if (left.length() == 0) {
			start.SetInt32(1);
		}
		else {
			start.SetBase16(left.c_str());
		}

		std::string right = s.substr(pos + 1);

		if (right[0] == '+') {
			Int t;
			t.SetBase16(right.substr(1).c_str());
			end.Set(&start);
			end.Add(&t);
		}
		else {
			end.SetBase16(right.c_str());
		}
	}

	return true;
}

#ifdef WIN64
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType) {
	case CTRL_C_EVENT:
		//printf("\n\nCtrl-C event\n\n");
		should_exit = true;
		return TRUE;

	default:
		return TRUE;
	}
}
#else
void CtrlHandler(int signum) {
	printf("\n\n  BYE\n");
	exit(signum);
}
#endif

int main(int argc, char** argv)
{
	//*****************************************************************************
	 
	enableAnsiCodes();  //что бы цвета работали в консоли
	setlocale(LC_ALL, "Russian");// Установка русской локали

	cout << zagolovok << endl;
	cout << "[+] =====================================================" << endl;
	cout << "[+] ROTOR-CUDA v" << RELEASE << endl;
	cout << "[+] =====================================================\n" << endl;
	cout << grey;

	if (argc == 1) {  // argc == 1 означает, что параметры командной строки отсутствуют
		printHelp();
		cout << text_close;
		cout << endl << "окно можно закрыть";
		cin.get();
		return 0;
	}

	//если запущен режим конвертации
	//-----------------------------------------------------------------
	if (strcmp(argv[1], "-convertFileETH") == 0) {
		// Установка русской локали
		setlocale(LC_ALL, "Russian");
		cout << zagolovok;
		cout << "[+] ********РЕЖИМ КОНВЕРТАЦИИ ETH********\n";
		cout << "\033[90m"; //голубой
		cout << "[+] АДРЕССА ВИДА:\n";
		cout << "[+] ETH\n";
		cout << "[+] 0xbC75b3f4Dc162C7a8aBF885822be0bcAa6FCeeB3\n";
		cout << "[+] bC75b3f4Dc162C7a8aBF885822be0bcAa6FCeeB3\n";
		cout << "[+] TRX\n";
		cout << "[+] TT9h3GV9ePs6CV4jcH4gB3jskb6zh6WC4e\n";
		cout << "\033[96m"; //голубой
		string infile = argv[2];

		// Создаем массив символов для хранения строки без расширения
		char outfile[256];
		char convfile[256] = "convfile.tmp";
		strncpy(outfile, infile.c_str(), sizeof(outfile));  // Копируем строку infile в outfile

		// Найдем последнюю точку в строке
		char* dot = strrchr(outfile, '.');

		// Проверим, что точка найдена и это именно ".txt"
		if (dot && strcmp(dot, ".txt") == 0) {
			// Заменяем расширение .txt на .bin
			strcpy(dot, ".bin");
		}

		// Печатаем результат
		cout << "\n[+] ВХОДНОЙ ФАЙЛ: " << white << infile << grey << " -> КОНВЕРТИРОВАННЫЙ+СОРТИРОВАНЫЙ: " << white << outfile << grey << endl;

		// Открытие входного файла
		std::ifstream stream(infile.c_str());
		cout << "[+] ОТКРЫВАЕМ ФАЙЛ: " << white << infile << grey << endl;
		if (!stream) {
			cout << "\033[31m"; //красный
			cout << "[-] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
			cerr << "[-] ОШИБКА,НЕ УДАЛОСЬ ОТКРЫТЬ ФАЙЛ: '" << infile << "'" << endl;
			cout << "[-] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
			cin.get();  // Ждем ввода пользователя
			// Аварийное завершение программы
			std::abort();
		}

		// Открытие выходного файла в бинарном режиме
		FILE* file = fopen(convfile, "wb");
		if (!file) {
			std::perror("[-] Ошибка при открытии выходного файла");
			return false;
		}

		string buffer;
		int nr = 0;
		int totalLines = 0;

		// Подсчитаем количество строк для вывода прогресса
		while (std::getline(stream, buffer)) {
			totalLines++;
		}
		stream.clear(); // сбрасываем состояние потока
		stream.seekg(0); // возвращаемся в начало файла

		cout << "[+] КОНВЕРТИРОВАНИЕ" << endl;
		hash160_20 h;
		memset(&h, 0, sizeof(h));
		try {
			while (std::getline(stream, buffer)) {

				// Проверка, что строка не пустая
				if (buffer.length() == 0) {
					continue;
				}


				//если строка длинее 34(trx) значить она eth
				if (buffer.size() == 34) {
					// Попытка декодирования Base58
					vector<unsigned char> binding;
					// Попытка декодирования Base58
					if (!DecodeBase58(buffer, binding)) {
						cerr << "[-] Ошибка декодирования Base58 на строке: " << nr + 1 << endl;
						continue; // Пропускаем этот адрес и переходим к следующему
					}
					// Проверка длины и копирование в массив размером 20 байт
					if (binding.size() >= 21) {
						std::array<unsigned char, 20> a;
						std::copy(binding.begin() + 1, binding.begin() + 21, a.begin());

						// Копирование 20 байтов в структуру хэша
						for (int i = 0; i < 20; i++) {
							h.bits20[i] = a[i];
						}
					}
				}
				else {
					if (buffer.size() > 2 && (buffer[0] == '0' && (buffer[1] == 'x' || buffer[1] == 'X'))) {
						buffer = buffer.substr(2); // Убираем "0x"   
					}

					// Проверка, что строка имеет корректную длину для 20 байт (40 символов в hex)
					if (buffer.length() != 40) {
						cerr << "[-] Некорректная длина строки: " << buffer << endl;
						continue;
					}

					// Конвертация hex строки в байты
					char buf[20] = { 0 };
					readHex(buf, buffer.c_str());

					// Копирование 20 байтов в структуру хэша
					for (int i = 0; i < 20; i++) {
						h.bits20[i] = buf[i];
					}
				}

				// Запись хэша в файл (в бинарном режиме)
				if (fwrite(h.bits20, 1, 20, file) != 20) {
					cerr << "[-] Ошибка записи в файл на строке: " << nr + 1 << endl;
					fclose(file);
					return false;
				}

				// Инкремент счётчика строк
				nr++;

				// Вывод обновляемого прогресса каждые 100000 строк
				if (nr % counterPROGRESS == 0 || nr == totalLines) {
					cout << "\r[+] " << nr << "/" << totalLines;
					cout.flush();  // обновляем строку без новой строки
				}
			}
		}
		catch (...) {
			cerr << "[-] Произошла ошибка при обработке файла." << endl;
			fclose(file);
			return false;
		}

		// Закрытие файлов
		fclose(file);
		stream.close();

		cout << endl; // Перевод строки после завершения работы
		cout << "[+] ГОТОВО\n" << endl;
		cout << "[+] СОРТИРОВКА:" << convfile << endl;


		sort_file(20, convfile, outfile);

		//удаляем временный convfile
		if (remove(convfile) != 0) {
			cout << "\033[31m"; //красный
			cout << "\n[+] НЕУДАЛОСЬ УДАЛИТЬ:\n" << convfile << " УДАЛИТЕ ВРУЧНУЮ" << endl;
		}

		cout << "\n[+] ГОТОВО:" << outfile << endl;

		// Ожидаем нажатия клавиши Enter, чтобы программа не закрывалась сразу
		cout << text_close;
		cout << "окно можно закрыть";
		cin.get();  // Ждем ввода пользователя
		return 0;
	}

	if (strcmp(argv[1], "-convertFileBTC") == 0) {
		// Установка русской локали
		setlocale(LC_ALL, "Russian");
		cout << "\033[92m"; //ярко-зелёный
		cout << "[+] ********РЕЖИМ КОНВЕРТАЦИИ BTC********\n";
		cout << "\033[90m"; //голубой
		cout << "[+] АДРЕССА ВИДА:\n";
		cout << "[+] BTC(и аналогичные ему)\n";
		cout << "[+] 17Xawwqd8bZiXcEKsCEFWVEad3TrmncPsF\n";
		cout << "[+] 3MDDvsiHEKGyyV4XXWbMT7mW5etzvczF3g\n";
		cout << "[+] bc1qfrsxj0g29y08msesvckqcsehanrtvfgkthjgmk\n";
		cout << "\033[96m"; //голубой
		string infile = argv[2];

		// Создаем массив символов для хранения строки без расширения
		char outfile[256];
		char convfile[256] = "convfile.tmp";
		strncpy(outfile, infile.c_str(), sizeof(outfile));  // Копируем строку infile в outfile

		// Найдем последнюю точку в строке
		char* dot = strrchr(outfile, '.');

		// Проверим, что точка найдена и это именно ".txt"
		if (dot && strcmp(dot, ".txt") == 0) {
			// Заменяем расширение .txt на .bin
			strcpy(dot, ".bin");
		}

		// Печатаем результат
		cout << "\n[+] ВХОДНОЙ ФАЙЛ: " << white << infile << grey << " -> КОНВЕРТИРОВАННЫЙ+СОРТИРОВАНЫЙ: " << white << outfile << grey << endl;

		// Открытие входного файла
		std::ifstream stream(infile.c_str());
		std::cout << "\n[+] ОТКРЫВАЕМ ФАЙЛ: " << white << infile << grey << endl;
		if (!stream) {
			std::cout << "\033[31m"; //красный
			std::cout << "[-] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << "\n";
			std::cerr << "[-] ОШИБКА,НЕ УДАЛОСЬ ОТКРЫТЬ ФАЙЛ: '" << infile << "'" << endl;
			std::cout << "[-] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << "\n";
			std::cin.get();  // Ждем ввода пользователя
			// Аварийное завершение программы
			std::abort();
		}

		// Открытие выходного файла в бинарном режиме
		FILE* file = fopen(convfile, "wb");
		if (!file) {
			std::perror("Ошибка при открытии выходного файла");
			return false;
		}

		std::string buffer;
		int nr = 0;
		int totalLines = 0;

		// Подсчитаем количество строк для вывода прогресса
		while (std::getline(stream, buffer)) {
			totalLines++;
		}
		stream.clear(); // сбрасываем состояние потока
		stream.seekg(0); // возвращаемся в начало файла

		cout << "\nКОНВЕРТИРОВАНИЕ\n" << endl;

		try {
			while (std::getline(stream, buffer)) {
				// Проверка, что строка не пустая
				if (buffer.length() == 0) {
					continue;
				}

				hash160_20 h;
				vector<unsigned char> r160;
				if (buffer.length() == 34) {
					if (DecodeBase58(buffer, r160)) {
						for (int i = 1; i <= 20 && i < r160.size(); i++) {
							h.bits20[i - 1] = r160.at(i);
						}
					}
				}
				else if (buffer.at(0) == 'b') {
					size_t data_len = 0;
					uint8_t* data = new uint8_t[64];
					if (bech32_decode_my(data, &data_len, buffer.c_str())) {
						for (int i = 0; i < data_len && i < 20; i++) {
							h.bits20[i] = data[i];
						}
					}
				}

				// Запись хэша в файл (в бинарном режиме)
				if (fwrite(h.bits20, 1, 20, file) != 20) {
					std::cerr << "Ошибка записи в файл на строке: " << nr + 1 << endl;
					fclose(file);
					return false;
				}

				// Инкремент счётчика строк
				nr++;

				// Вывод обновляемого прогресса каждые 100000 строк
				if (nr % 1000000 == 0 || nr == totalLines) {
					cout << "\rПрогресс: " << nr << "/" << totalLines;
					cout.flush();  // обновляем строку без новой строки
				}
			}
		}
		catch (...) {
			std::cerr << "Произошла ошибка при обработке файла." << endl;
			fclose(file);
			return false;
		}

		// Закрытие файлов
		fclose(file);
		stream.close();

		cout << endl; // Перевод строки после завершения работы
		cout << "[+] ГОТОВО\n" << endl;
		cout << "[+] СОРТИРОВКА:" << convfile << endl;


		sort_file(20, convfile, outfile);

		//удаляем временный convfile
		if (remove(convfile) != 0) {
			cout << "\033[31m"; //красный
			cout << "\n[+] НЕУДАЛОСЬ УДАЛИТЬ:\n" << convfile << " УДАЛИТЕ ВРУЧНУЮ" << endl;
		}

		cout << "\n[+] ГОТОВО:" << outfile << endl;

		// Ожидаем нажатия клавиши Enter, чтобы программа не закрывалась сразу
		cout << text_close;
		cout << "окно можно закрыть";
		cin.get();  // Ждем ввода пользователя
		return 0;
	}
	//------------------------------------------------------------------

	//*****************************************************************************


	// Global Init
	Timer::Init();
	rseed(Timer::getSeed32());
		
	bool gpuEnable = false;
	bool gpuAutoGrid = true;
	int compMode = SEARCH_COMPRESSED;
	vector<int> gpuId = { 0 };
	vector<int> gridSize;
	int nbit2 = 0;
	int display = 1;
	int zet = 0;
	string outputFile = "Found.txt";

	string inputFile = "";	// for both multiple hash160s and x points
	string address = "";	// for single address mode
	string xpoint = "";		// for single x point mode

	std::vector<unsigned char> hashORxpoint;
	bool singleAddress = false;
	int nbCPUThread = Timer::getCoreNumber();

	bool tSpecified = false;
	bool useSSE = true;
	uint32_t maxFound = 1024 * 64;

	uint64_t rKey = 0;
	int next = 0;
	Int rangeStart;
	Int rangeEnd;
	rangeStart.SetInt32(0);
	rangeEnd.SetInt32(0);

	int searchMode = 0;
	int coinType = COIN_BTC;

	hashORxpoint.clear();

	// cmd args parsing
	CmdParse parser;
	parser.add("-h", "--help", false);
	parser.add("-c", "--check", false);
	parser.add("-l", "--list", false);
	parser.add("-u", "--uncomp", false);
	parser.add("-b", "--both", false);
	parser.add("-g", "--gpu", false);
	parser.add("", "--gpui", true);
	parser.add("", "--gpux", true);
	parser.add("-t", "--thread", true);
	parser.add("-i", "--in", true);
	parser.add("-o", "--out", true);
	parser.add("-m", "--mode", true);
	parser.add("", "--coin", true);
	parser.add("", "--range", true);
	parser.add("-r", "--rkey", true);
	parser.add("-v", "--version", false);
	parser.add("-n", "--next", true);
	parser.add("-z", "--zet", true);
	parser.add("-d", "--display", true);
	if (argc == 1) {
		usage();
		return 0;
	}
	try {
		parser.parse(argc, argv);
	}
	catch (std::string err) {
		printf("  Error: %s\n", err.c_str());
		usage();
		exit(-1);
	}
	std::vector<OptArg> args = parser.getArgs();

	for (unsigned int i = 0; i < args.size(); i++) {
		OptArg optArg = args[i];
		std::string opt = args[i].option;

		try {
			if (optArg.equals("-h", "--help")) {
				usage();
				return 0;
			}
			else if (optArg.equals("-c", "--check")) {
				printf("\n  Checking... Secp256K1\n\n");
				Secp256K1* secp = new Secp256K1();
				secp->Init();
				secp->Check();
				printf("\n\n  Checking... Int\n\n");
				Int* K = new Int();
				K->SetBase16("3EF7CEF65557B61DC4FF2313D0049C584017659A32B002C105D04A19DA52CB47");
				K->Check();
				delete secp;
				delete K;
				printf("\n\n  Checked successfully\n\n");
				return 0;
			}
			else if (optArg.equals("-l", "--list")) {
#ifdef WIN64
				GPUEngine::PrintCudaInfo();
#else
				printf("  GPU code not compiled, use -DWITHGPU when compiling.\n");
#endif
				return 0;
			}
			else if (optArg.equals("-u", "--uncomp")) {
				compMode = SEARCH_UNCOMPRESSED;
			}
			else if (optArg.equals("-b", "--both")) {
				compMode = SEARCH_BOTH;
			}
			else if (optArg.equals("-g", "--gpu")) {
				gpuEnable = true;
				nbCPUThread = 0;
			}
			else if (optArg.equals("", "--gpui")) {
				string ids = optArg.arg;
				getInts("--gpui", gpuId, ids, ',');
			}
			else if (optArg.equals("", "--gpux")) {
				string grids = optArg.arg;
				getInts("--gpux", gridSize, grids, ',');
				gpuAutoGrid = false;
			}
			else if (optArg.equals("-t", "--thread")) {
				nbCPUThread = std::stoi(optArg.arg);
				tSpecified = true;
			}
			else if (optArg.equals("-i", "--in")) {
				inputFile = optArg.arg;
			}
			else if (optArg.equals("-o", "--out")) {
				outputFile = optArg.arg;
			}
			else if (optArg.equals("-m", "--mode")) {
				searchMode = parseSearchMode(optArg.arg);
			}
			else if (optArg.equals("", "--coin")) {
				coinType = parseCoinType(optArg.arg);
			}
			else if (optArg.equals("", "--range")) {
				std::string range = optArg.arg;
				parseRange(range, rangeStart, rangeEnd);
			}
			else if (optArg.equals("-r", "--rkey")) {
				rKey = std::stoull(optArg.arg);
			}
			else if (optArg.equals("-n", "--next")) {
				next = std::stoull(optArg.arg);
			}
			else if (optArg.equals("-z", "--zet")) {
				zet = std::stoull(optArg.arg);
			}
			else if (optArg.equals("-d", "--display")) {
				display = std::stoull(optArg.arg);
			}
			else if (optArg.equals("-v", "--version")) {
				return 0;
			}
		}
		catch (std::string err) {
			printf("Error: %s\n", err.c_str());
			usage();
			return -1;
		}
	}

	if (coinType == COIN_ETH && (searchMode == SEARCH_MODE_SX || searchMode == SEARCH_MODE_MX/* || compMode == SEARCH_COMPRESSED*/)) {
		printf("  Error: %s\n", "  Wrong search or compress mode provided for ETH coin type");
		usage();
		return -1;
	}
	if (coinType == COIN_ETH) {
		compMode = SEARCH_UNCOMPRESSED;
		useSSE = false;
	}
	if (searchMode == (int)SEARCH_MODE_MX || searchMode == (int)SEARCH_MODE_SX)
		useSSE = false;


	// Parse operands
	std::vector<std::string> ops = parser.getOperands();

	if (ops.size() == 0) {
		// read from file
		if (inputFile.size() == 0) {
			printf("  Error: %s\n", "  Missing arguments");
			usage();
			return -1;
		}
		if (searchMode != SEARCH_MODE_MA && searchMode != SEARCH_MODE_MX) {
			printf("  Error: %s\n", "  Wrong search mode provided for multiple addresses or xpoints");
			usage();
			return -1;
		}
	}
	else {
		// read from cmdline
		if (ops.size() != 1) {
			printf("  Error: %s\n", "  Wrong args or more than one address or xpoint are provided, use inputFile for multiple addresses or xpoints");
			usage();
			return -1;
		}
		if (searchMode != SEARCH_MODE_SA && searchMode != SEARCH_MODE_SX) {
			printf("  Error: %s\n", "  Wrong search mode provided for single address or xpoint");
			usage();
			return -1;
		}


		switch (searchMode) {
		case (int)SEARCH_MODE_SA:
		{
			address = ops[0];
			if (coinType == COIN_BTC) {
				if (address.length() < 30 || address[0] != '1') {
					printf("  Error: %s\n", "  Invalid address, must have Bitcoin P2PKH address or Ethereum address");
					usage();
					return -1;
				}
				else {
					if (DecodeBase58(address, hashORxpoint)) {
						hashORxpoint.erase(hashORxpoint.begin() + 0);
						hashORxpoint.erase(hashORxpoint.begin() + 20, hashORxpoint.begin() + 24);
						assert(hashORxpoint.size() == 20);
					}
				}
			}
			else {
				if (address.length() != 42 || address[0] != '0' || address[1] != 'x') {
					printf("  Error: %s\n", "  Invalid Ethereum address");
					usage();
					return -1;
				}
				address.erase(0, 2);
				for (int i = 0; i < 40; i += 2) {
					uint8_t c = 0;
					for (size_t j = 0; j < 2; j++) {
						uint32_t c0 = (uint32_t)address[i + j];
						uint8_t c2 = (uint8_t)strtol((char*)&c0, NULL, 16);
						if (j == 0)
							c2 = c2 << 4;
						c |= c2;
					}
					hashORxpoint.push_back(c);
				}
				assert(hashORxpoint.size() == 20);
			}
		}
		break;
		case (int)SEARCH_MODE_SX:
		{
			unsigned char xpbytes[32];
			xpoint = ops[0];
			Int* xp = new Int();
			xp->SetBase16(xpoint.c_str());
			xp->Get32Bytes(xpbytes);
			for (int i = 0; i < 32; i++)
				hashORxpoint.push_back(xpbytes[i]);
			delete xp;
			if (hashORxpoint.size() != 32) {
				printf("  Error: %s\n", "  Invalid xpoint");
				usage();
				return -1;
			}
		}
		break;
		default:
			printf("  Error: %s\n", "  Invalid search mode for single address or xpoint");
			usage();
			return -1;
			break;
		}
	}

	if (gridSize.size() == 0) {
		for (int i = 0; i < gpuId.size(); i++) {
			gridSize.push_back(-1);
			gridSize.push_back(128);
		}
	}
	if (gridSize.size() != gpuId.size() * 2) {
		printf("  Error: %s\n", "  Invalid gridSize or gpuId argument, must have coherent size\n");
		usage();
		return -1;
	}

	//if (rangeStart.GetBitLength() <= 0) {
		//printf("  Error: %s\n", "  Invalid start range, provide start range at least, end range would be: start range + 0xFFFFFFFFFFFFULL\n");
		//usage();
		//return -1;
	//}

	if (nbCPUThread > 0 && gpuEnable) {
		printf("  Error: %s\n", "  Invalid arguments, CPU and GPU, both can't be used together right now\n");
		usage();
		return -1;
	}

	// Let one CPU core free per gpu is gpu is enabled
	// It will avoid to hang the system
	if (!tSpecified && nbCPUThread > 1 && gpuEnable)
		nbCPUThread -= (int)gpuId.size();
	if (nbCPUThread < 0)
		nbCPUThread = 0;

	int color = 0;
	int bok1 = 0;
	int bok2 = 0;
	if (display > 0) {

		color = color + 15;
		bok1 = bok1 + 220;
		bok2 = bok2 + 1000;
	}
	else {
		color = color + 14;
		bok1 = bok1 + 220;
		bok2 = bok2 + 1000;
	}

	if (coinType == COIN_BTC)
		printf("[+] COMP MODE    : %s\n", compMode == SEARCH_COMPRESSED ? "COMPRESSED" : (compMode == SEARCH_UNCOMPRESSED ? "UNCOMPRESSED" : "COMPRESSED & UNCOMPRESSED"));
	printf("[+] COIN TYPE    : %s\n", coinType == COIN_BTC ? "BITCOIN" : "ETHEREUM");
	printf("[+] SEARCH MODE  : %s\n", searchMode == (int)SEARCH_MODE_MA ? "Multi Address" : (searchMode == (int)SEARCH_MODE_SA ? "Single Address" : (searchMode == (int)SEARCH_MODE_MX ? "Multi X Points" : "Single X Point")));
	printf("[+] DEVICE       : %s\n", (gpuEnable && nbCPUThread > 0) ? "  CPU & GPU" : ((!gpuEnable && nbCPUThread > 0) ? "CPU" : "GPU"));
	printf("[+] CPU THREAD   : %d\n", nbCPUThread);
	nbit2 += nbCPUThread;
	if (gpuEnable) {
		printf("[+] GPU IDS      : ");
		for (int i = 0; i < gpuId.size(); i++) {
			printf("%d", gpuId.at(i));
			if (i + 1 < gpuId.size())
				printf(", ");
		}
		printf("\n");
		printf("[+] GPU GRIDSIZE : ");
		for (int i = 0; i < gridSize.size(); i++) {
			printf("%d", gridSize.at(i));
			if (i + 1 < gridSize.size()) {
				if ((i + 1) % 2 != 0) {
					printf("x");
				}
				else {
					printf(", ");
				}

			}
		}
		if (gpuAutoGrid)
			printf(" (Auto grid size)\n");
		else
			printf("\n");
	}
	printf("[+] SSE          : %s\n", useSSE ? "YES" : "NO");
	
	if (coinType == COIN_BTC) {
		switch (searchMode) {
		case (int)SEARCH_MODE_MA:
			printf("[+] BTC HASH160s : %s\n", inputFile.c_str());
			break;
		case (int)SEARCH_MODE_SA:
			printf("[+] BTC ADDRESS  : %s\n", address.c_str());
			break;
		case (int)SEARCH_MODE_MX:
			printf("[+] BTC XPOINTS  : %s\n", inputFile.c_str());
			break;
		case (int)SEARCH_MODE_SX:
			printf("[+] BTC XPOINT   : %s\n", xpoint.c_str());
			break;
		default:
			break;
		}
	}
	else {
		switch (searchMode) {
		case (int)SEARCH_MODE_MA:
			printf("[+] ETH ADDRESSES: %s\n", inputFile.c_str());
			break;
		case (int)SEARCH_MODE_SA:
			printf("[+] ETH ADDRESS  : 0x%s\n", address.c_str());
			break;
		default:
			break;
		}
	}
	printf("[+] OUTPUT FILE  : %s\n", outputFile.c_str());
	
#ifdef WIN64
	if (SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
		Rotor* v;
		switch (searchMode) {
		case (int)SEARCH_MODE_MA:
		case (int)SEARCH_MODE_MX:
			v = new Rotor(inputFile, compMode, searchMode, coinType, gpuEnable, outputFile, useSSE,
				maxFound, rKey, nbit2, next, zet, display, rangeStart.GetBase16(), rangeEnd.GetBase16(), should_exit);
			break;
		case (int)SEARCH_MODE_SA:
		case (int)SEARCH_MODE_SX:
			v = new Rotor(hashORxpoint, compMode, searchMode, coinType, gpuEnable, outputFile, useSSE,
				maxFound, rKey, nbit2, next, zet, display, rangeStart.GetBase16(), rangeEnd.GetBase16(), should_exit);
			break;
		default:
			printf("\n\n[E] Nothing to do, exiting\n");
			return 0;
			break;
		}
		v->Search(nbCPUThread, gpuId, gridSize, should_exit);
		delete v;
		printf("\n\n  BYE\n");
		return 0;
	}
	else {
		printf("  Error: could not set control-c handler\n");
		return -1;
	}
#else
	signal(SIGINT, CtrlHandler);
	Rotor* v;
	switch (searchMode) {
	case (int)SEARCH_MODE_MA:
	case (int)SEARCH_MODE_MX:
		v = new Rotor(inputFile, compMode, searchMode, coinType, gpuEnable, outputFile, useSSE,
			maxFound, rKey, nbit2, next, zet, display, rangeStart.GetBase16(), rangeEnd.GetBase16(), should_exit);
		break;
	case (int)SEARCH_MODE_SA:
	case (int)SEARCH_MODE_SX:
		v = new Rotor(hashORxpoint, compMode, searchMode, coinType, gpuEnable, outputFile, useSSE,
			maxFound, rKey, nbit2, next, zet, display, rangeStart.GetBase16(), rangeEnd.GetBase16(), should_exit);
		break;
	default:
		printf("\n\n  Nothing to do, exiting\n");
		return 0;
		break;
	}
	v->Search(nbCPUThread, gpuId, gridSize, should_exit);
	delete v;
	return 0;
#endif
}