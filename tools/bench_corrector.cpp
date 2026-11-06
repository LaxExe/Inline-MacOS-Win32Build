// Accuracy + latency harness for the Tier-1 corrector.
// Build: see tools target `bench_corrector` in tools/CMakeLists (or compile manually against core_lib).
#include "core/tier1/corrector.h"
#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#ifdef __APPLE__
#include <mach/mach.h>
#endif

int main(int argc, char** argv) {
    SymSpellDict dict;
    const char* path = argc > 1 ? argv[1] : "dictionaries/frequency_dictionary_en_82_765.txt";
    auto t0 = std::chrono::steady_clock::now();
    if (!dict.loadDictionary(path, 0, 1)) { std::cerr << "dict load failed\n"; return 1; }
    dict.loadBigramDictionary("dictionaries/frequency_bigramdictionary_en_243_342.txt");
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "dictionaries load (unigrams + bigrams): "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << " ms\n";

    AppConfig cfg;
    cfg.maxDictionaryEditDistance = 2;

    struct TestCase {
        std::wstring typo;
        std::wstring want;
        std::wstring prevWord = L"";
    };

    const std::vector<TestCase> cases = {
        {L"recieve", L"receive"}, {L"definately", L"definitely"}, {L"seperate", L"separate"},
        {L"wierd", L"weird"}, {L"becuase", L"because"}, {L"beacuse", L"because"},
        {L"tommorow", L"tomorrow"}, {L"untill", L"until"}, {L"adress", L"address"},
        {L"occured", L"occurred"}, {L"thier", L"their"}, {L"freind", L"friend"},
        {L"goverment", L"government"}, {L"begining", L"beginning"}, {L"accomodate", L"accommodate"},
        {L"existance", L"existence"}, {L"wich", L"which"}, {L"alot", L"a lot"},
        {L"teh", L"the"}, {L"adn", L"and"}, {L"taht", L"that"}, {L"jsut", L"just"},
        {L"waht", L"what"}, {L"hte", L"the"}, {L"yuo", L"you"}, {L"cna", L"can"},
        {L"tehy", L"they"}, {L"realy", L"really"}, {L"thsi", L"this"}, {L"abou", L"about"},
        {L"wiht", L"with"}, {L"ahve", L"have"}, {L"wih", L"with"}, {L"fo", L"of"},
        {L"computr", L"computer"}, {L"keybord", L"keyboard"}, {L"progam", L"program"},
        {L"langauge", L"language"}, {L"importnat", L"important"}, {L"diffrent", L"different"},
        {L"probaly", L"probably"}, {L"sentance", L"sentence"}, {L"writting", L"writing"},
        {L"wnat", L"want"}, {L"thnk", L"think"}, {L"peopel", L"people"}, {L"becasue", L"because"},
        {L"thigns", L"things"}, {L"somthing", L"something"}, {L"evrything", L"everything"},
        {L"quikly", L"quickly"}, {L"mesage", L"message"}, {L"emial", L"email"},
        {L"mispelled", L"misspelled"}, {L"tpyo", L"typo"}, {L"wrods", L"words"},
        {L"corect", L"correct"}, {L"smal", L"small"}, {L"gret", L"great"},
        // Helo disambiguation based on context:
        {L"helo", L"hello", L""},
        {L"helo", L"hello", L"say"},
        {L"helo", L"help", L"need"},
        {L"helo", L"help", L"please"},
        {L"helo", L"help", L"for"},
        {L"helo", L"help", L"can"},
        {L"helo", L"help", L"some"},
        // Additional context disambiguations:
        {L"frm", L"from", L"away"},
        {L"frm", L"form", L"application"}
    };

    int missed = 0;
    double totalUs = 0, worstUs = 0;
    for (const auto& tc : cases) {
        auto a = std::chrono::steady_clock::now();
        auto r = correctWord(tc.typo, dict, cfg, false, tc.prevWord);
        auto b = std::chrono::steady_clock::now();
        double us = std::chrono::duration<double, std::micro>(b - a).count();
        totalUs += us;
        if (us > worstUs) worstUs = us;
        bool ok = r.didCorrect && r.corrected == tc.want;
        if (!ok) {
            ++missed;
            std::wcout << L"MISS  " << (tc.prevWord.empty() ? L"" : tc.prevWord + L" ") << tc.typo
                       << L" -> " << (r.didCorrect ? r.corrected : L"(unchanged)")
                       << L"   (wanted " << tc.want << L")\n";
        }
    }
    std::cout << "missed " << missed << " / " << cases.size() << "\n";
    std::cout << "avg typo correction lookup: " << totalUs / cases.size() << " us, worst: " << worstUs << " us\n";

    // Microbenchmark valid words, slang, and expansions
    const std::vector<std::wstring> validWords = {
        L"the", L"repo", L"gng", L"brb", L"ttyl", L"idk", L"ngl", L"tbh",
        L"apple", L"system", L"program", L"keyboard", L"computer"
    };

    double validTotalUs = 0;
    const int kIterations = 50000;
    for (int i = 0; i < kIterations; ++i) {
        const auto& w = validWords[i % validWords.size()];
        auto a = std::chrono::steady_clock::now();
        auto r = correctWord(w, dict, cfg);
        auto b = std::chrono::steady_clock::now();
        validTotalUs += std::chrono::duration<double, std::micro>(b - a).count();
    }
    std::cout << "valid words / slang lookup avg: " << (validTotalUs / kIterations) << " us\n";

    // Microbenchmark expansions like "og" -> "of", "fo" -> "of", "dont" -> "don't"
    const std::vector<std::wstring> expansionWords = { L"og", L"fo", L"dont", L"cant", L"im", L"alot" };
    double expTotalUs = 0;
    for (int i = 0; i < kIterations; ++i) {
        const auto& w = expansionWords[i % expansionWords.size()];
        auto a = std::chrono::steady_clock::now();
        auto r = correctWord(w, dict, cfg);
        auto b = std::chrono::steady_clock::now();
        expTotalUs += std::chrono::duration<double, std::micro>(b - a).count();
    }
    std::cout << "expansion / QWERTY slip ('og', 'dont') lookup avg: " << (expTotalUs / kIterations) << " us\n";

    // Microbenchmark run-together splitting
    const std::vector<std::wstring> runTogetherWords = { L"whenI", L"twowords", L"theclass", L"ofevening" };
    double splitTotalUs = 0;
    for (int i = 0; i < kIterations; ++i) {
        const auto& w = runTogetherWords[i % runTogetherWords.size()];
        auto a = std::chrono::steady_clock::now();
        auto r = correctWord(w, dict, cfg);
        auto b = std::chrono::steady_clock::now();
        splitTotalUs += std::chrono::duration<double, std::micro>(b - a).count();
    }
    std::cout << "run-together split ('whenI', 'twowords') lookup avg: " << (splitTotalUs / kIterations) << " us\n";

#ifdef __APPLE__
    mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
        double rssMb = static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
        std::cout << "resident memory footprint (RSS): " << rssMb << " MB\n";
    }
#endif

    return 0;
}
