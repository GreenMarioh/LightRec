#include "../../src/config/Config.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

void test_defaults() {
    Config c = Config::defaults();
    assert(c.clipDurationSec > 0);
    assert(c.outputDir == Config::defaultOutputDir());
}

void test_save_load_roundtrip() {
    Config c = Config::defaults();
    c.clipDurationSec = 120;
    c.fps = 120;
    c.preferredEncoder = EncoderType::NVENC;
    
    auto tempPath = std::filesystem::temp_directory_path() / "test_config_roundtrip.json";
    c.save(tempPath);
    
    Config loaded = Config::load(tempPath);
    assert(loaded.clipDurationSec == 120);
    assert(loaded.fps == 120);
    assert(loaded.preferredEncoder == EncoderType::NVENC);
    
    std::filesystem::remove(tempPath);
}

void test_missing_file() {
    auto tempPath = std::filesystem::temp_directory_path() / "test_config_missing.json";
    std::filesystem::remove(tempPath);
    
    Config c = Config::load(tempPath);
    Config def = Config::defaults();
    assert(c.clipDurationSec == def.clipDurationSec);
    assert(c.fps == def.fps);
}

void test_partial_json() {
    auto tempPath = std::filesystem::temp_directory_path() / "test_config_partial.json";
    std::ofstream out(tempPath);
    out << R"({"clipDurationSec": 999, "preferredEncoder": 1})"; // 1 is QSV
    out.close();
    
    Config c = Config::load(tempPath);
    Config def = Config::defaults();
    assert(c.clipDurationSec == 999);
    assert(c.fps == def.fps);
    assert(c.preferredEncoder == EncoderType::QSV);
    
    std::filesystem::remove(tempPath);
}

int main() {
    test_defaults();
    test_save_load_roundtrip();
    test_missing_file();
    test_partial_json();
    std::cout << "Config tests passed\n";
    return 0;
}
