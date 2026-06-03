#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <curl/curl.h>

static size_t writeCallback(void *data, size_t size, size_t nmemb, std::string *out) {
    out->append((char *)data, size * nmemb);
    return size * nmemb;
}

static std::string httpGet(const std::string &url) {
    CURL *curl = curl_easy_init();
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return body;
}

// pull a string value for a given key from a JSON snippet
static std::string extractStr(const std::string &s, const std::string &key) {
    std::string k = "\"" + key + "\"";
    auto p = s.find(k);
    if (p == std::string::npos) return "";
    p += k.size();
    while (p < s.size() && (s[p] == ' ' || s[p] == ':')) p++;
    if (s.substr(p, 4) == "null") return "";
    if (s[p] != '"') return "";
    p++;
    std::string val;
    while (p < s.size() && s[p] != '"') {
        if (s[p] == '\\') { p++; }
        val += s[p++];
    }
    return val;
}

static long long extractInt(const std::string &s, const std::string &key) {
    std::string k = "\"" + key + "\"";
    auto p = s.find(k);
    if (p == std::string::npos) return 0;
    p += k.size();
    while (p < s.size() && (s[p] == ' ' || s[p] == ':')) p++;
    if (s.substr(p, 4) == "null") return 0;
    long long v = 0;
    while (p < s.size() && s[p] >= '0' && s[p] <= '9')
        v = v * 10 + (s[p++] - '0');
    return v;
}

struct Article {
    std::string name;
    long long comments;
};

static std::vector<Article> parsePage(const std::string &json) {
    std::vector<Article> result;

    auto dataStart = json.find("\"data\"");
    if (dataStart == std::string::npos) return result;
    auto arr = json.find('[', dataStart);
    if (arr == std::string::npos) return result;

    size_t i = arr + 1;
    int depth = 0;
    size_t objStart = std::string::npos;

    while (i < json.size()) {
        if (json[i] == '{') {
            if (depth == 0) objStart = i;
            depth++;
        } else if (json[i] == '}') {
            depth--;
            if (depth == 0 && objStart != std::string::npos) {
                std::string obj = json.substr(objStart, i - objStart + 1);
                std::string title = extractStr(obj, "title");
                std::string stitle = extractStr(obj, "story_title");
                std::string name = title.empty() ? stitle : title;
                if (!name.empty())
                    result.push_back({name, extractInt(obj, "num_comments")});
                objStart = std::string::npos;
            }
        } else if (json[i] == ']' && depth == 0) {
            break;
        }
        i++;
    }
    return result;
}

std::vector<std::string> topArticles(int limit) {
    const std::string base = "https://jsonmock.hackerrank.com/api/articles?page=";

    std::string first = httpGet(base + "1");
    long long pages = extractInt(first, "total_pages");

    std::vector<Article> all;
    auto p1 = parsePage(first);
    all.insert(all.end(), p1.begin(), p1.end());

    for (long long pg = 2; pg <= pages; pg++) {
        auto arts = parsePage(httpGet(base + std::to_string(pg)));
        all.insert(all.end(), arts.begin(), arts.end());
    }

    std::sort(all.begin(), all.end(), [](const Article &a, const Article &b) {
        if (a.comments != b.comments) return a.comments > b.comments;
        return a.name > b.name;
    });

    std::vector<std::string> top;
    for (int i = 0; i < limit && i < (int)all.size(); i++)
        top.push_back(all[i].name);
    return top;
}

int main() {
    auto results = topArticles(2);
    for (auto &r : results)
        std::cout << r << "\n";
    return 0;
}
