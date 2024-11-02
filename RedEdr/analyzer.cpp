

#include <iostream>
#include <sstream>
#include <vector>
#include <locale>
#include <codecvt>

#include "eventproducer.h"
#include "config.h"
#include "logging.h"
#include "utils.h"
#include "json.hpp"
#include "analyzer.h"


HANDLE analyzer_thread;

MyAnalyzer g_Analyzer;


std::string CriticalityToString(Criticality c) {
    switch (c) {
    case Criticality::LOW:   return "LOW";
    case Criticality::MEDIUM: return "MEDIUM";
    case Criticality::HIGH:  return "HIGH";
    default:          return "UNKNOWN";
    }
}

void PrintEvent(nlohmann::json j) {
    std::string output = j.dump();
    LOG_A(LOG_INFO, "Event: %s", output.c_str());
}

//


void MyAnalyzer::ResetData() {
	detections.clear();
}


std::string MyAnalyzer::GetAllDetectionsAsJson() {
    nlohmann::json jsonArray = detections;
    return jsonArray.dump();
}


void MyAnalyzer::AnalyzerNewDetection(Criticality c, std::string s) {
    std::string o = CriticalityToString(c) + ": " + s;
    detections.push_back(o);
}



uint64_t AlignToPage(uint64_t addr) {
    constexpr uint64_t pageSize = 4096;       // Typically 4 KB
    return addr & ~(pageSize - 1);            // Aligns down to nearest page boundary
}


void MyAnalyzer::AnalyzeEventJson(nlohmann::json j) {
    // Parse event
    BOOL printed = FALSE;

    //std::string protectStr = j["protect"].get<std::string>();
    //std::string callstackStr = j["callstack"].dump();
    if (j["type"] == "loaded_dll") {
        //std::cout << j["dlls"];
        for (const auto& it: j["dlls"]) {
            uint64_t addr = std::stoull(it["addr"].get<std::string>(), nullptr, 16);
            uint64_t size = std::stoull(it["size"].get<std::string>(), nullptr, 16);
            std::string protection = "???";
            std::string name = it["name"];

			addr = AlignToPage(addr);
            MemoryRegion* region = new MemoryRegion(name, addr, size, protection);
            targetInfo.AddMemoryRegion(addr, region);
        }
    }

    if (j["type"] == "dll" && j["func"] == "AllocateVirtualMemory") {
        uint64_t addr = std::stoull(j["base_addr"].get<std::string>(), nullptr, 16);
        uint64_t size = std::stoull(j["size"].get<std::string>(), nullptr, 16);
		std::string protection = j["protect"];
		std::string name = "Allocate";

        addr = AlignToPage(addr);
		MemoryRegion* region = new MemoryRegion(name, addr, size, protection);
		targetInfo.AddMemoryRegion(addr, region);
    }

    if (j["type"] == "dll" && j["func"] == "ProtectVirtualMemory") {
        uint64_t addr = std::stoull(j["base_addr"].get<std::string>(), nullptr, 16);
        uint64_t size = std::stoull(j["size"].get<std::string>(), nullptr, 16);
        std::string protection = j["protect"];
        std::string name = "Protect";

        addr = AlignToPage(addr);
        // Check if exists
		if (targetInfo.ExistMemoryRegion(addr) == NULL) {
			LOG_A(LOG_WARNING, "Analyzer: ProtectVirtualMemory region not found");
            //MemoryRegion* region = new MemoryRegion(name, addr, size, protection);
            //targetInfo.AddMemoryRegion(addr, region);
        }
		else {
			// Update protection
			MemoryRegion* region = targetInfo.GetMemoryRegion(addr);
			region->protection += ";" + protection;
			//LOG_A(LOG_INFO, "Analyzer: ProtectVirtualMemory: %s 0x%llx 0x%llx %s",
			//	name.c_str(), addr, size, protection.c_str());

        }
    }

    // Allocate or map memory with RWX protection
    if (j["protect"] == "RWX") {
        j["detection"] += "RWX";
        Criticality c = Criticality::HIGH;

        std::stringstream ss;
        ss << "Analyzer: Function " << j["func"].get<std::string>() << " doing RWX";
        ss << " with size " << j["size"].get<std::string>();

        if (j["func"] == "MapViewOfSection") {
            ss << " SectionHandle " << j["section_handle"].get<std::string>();
            c = Criticality::LOW;
        }
        else if (j["func"] == "ProtectVirtualMemory") {
            //ss << " SectionHandle: " << j["section_handle"].get<std::string>();
            c = Criticality::HIGH;
        }
        AnalyzerNewDetection(c, ss.str());
        printed = TRUE;
    }

    int idx = 0;
    // Check callstack
    for (const auto& callstack_entry : j["callstack"]) {
        CriticalityManager cm;
        std::stringstream ss;
        BOOL print2 = FALSE;

        // Callstack entry from RWX region
        if (callstack_entry["protect"] == "RWX") {
            ss << "High: RWX section, ";
            j["detection"] += "RWX";
            cm.set(Criticality::HIGH);
            print2 = TRUE;
        }

        // Callstack entry from non-image region
        if (callstack_entry["type"] != "IMAGE") { // MEM_IMAGE
            if (callstack_entry["type"] == "MAPPED") { // MEM_MAPPED
                ss << "Low: MEM_MAPPED section, ";
                j["detection"] += "MEM_MAPPED";
                cm.set(Criticality::LOW);
                print2 = TRUE;
            }
            else if (callstack_entry["type"] == "PRIVATE") { // MEM_PRIVATE, unbacked!
                ss << "High: MEM_PRIVATE section, ";
                j["detection"] += "MEM_PRIVATE";
                cm.set(Criticality::HIGH);
                print2 = TRUE;
            }
            else {
                ss << "Unknown: other section, ";
                j["detection"] += "MEM_OTHER";  // TODO: add hex
                cm.set(Criticality::MEDIUM);
                print2 = TRUE;
            }
        }

        if (print2) {
            if (!printed) {
                std::stringstream x;
                x << "Function " << j["func"].get<std::string>();
				printed = TRUE;
            }

            std::stringstream s;
            s << "Analyzer: Suspicious callstack " << idx << " of " << j["callstack"].size() << " by " << j["func"].get<std::string>();
            if (j["func"] == "ProtectVirtualMemory") {
				s << " destination " << j["base_addr"].get<std::string>();
                s << " protect " << j["protect"].get<std::string>();
			}
            s << " addr " << callstack_entry["addr"].get<std::string>();
            s << " protect " << callstack_entry["protect"].get<std::string>();
            s << " type " << callstack_entry["type"].get<std::string>();
            AnalyzerNewDetection(cm.get(), s.str());
        }
        idx += 1;
    }

}


void MyAnalyzer::AnalyzeEventStr(std::string eventStr) {
    //std::cout << L"Processing: " << eventStr << std::endl;
    nlohmann::json j;
    try
    {
        j = nlohmann::json::parse(eventStr);
    }
    catch (const nlohmann::json::exception& e)
    {
        LOG_A(LOG_WARNING, "JSON Parser Exception msg: %s", e.what());
        LOG_A(LOG_WARNING, "JSON Parser Exception event: %s", eventStr.c_str());
        return;
    }

    AnalyzeEventJson(j);

}

DWORD WINAPI AnalyzerThread(LPVOID param) {
    LOG_A(LOG_INFO, "!Analyzer: Start thread");
    size_t arrlen = 0;
    std::unique_lock<std::mutex> lock(g_EventProducer.analyzer_shutdown_mtx);

    while (true) {
        // Block for new events
        g_EventProducer.cv.wait(lock, [] { return g_EventProducer.HasMoreEvents() || g_EventProducer.done; });

        // get em events
        std::vector<std::string> output_entries = g_EventProducer.GetEventsFrom();

        // handle em
        arrlen = output_entries.size();
        for (std::string& entry : output_entries) {
            g_Analyzer.AnalyzeEventStr(entry);
        }
    }

    LOG_A(LOG_INFO, "!Analyzer: Exit thread");
    return 0;
}


int InitializeAnalyzer(std::vector<HANDLE>& threads) {
    analyzer_thread = CreateThread(NULL, 0, AnalyzerThread, NULL, 0, NULL);
    if (analyzer_thread == NULL) {
        LOG_A(LOG_ERROR, "WEB: Failed to create thread for webserver");
        return 1;
    }
    threads.push_back(analyzer_thread);
    return 0;
}


void StopAnalyzer() {
    if (analyzer_thread != NULL) {
    }
}
