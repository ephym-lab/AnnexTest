#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <curl/curl.h>

static size_t writeCallback(void *data, size_t size, size_t nmemb, std::string *out) {
    out->append((char *)data, size * nmemb);
    return size * nmemb;
}

static std::string httpGet(const std::string &url) {
    CURL *curl = curl_easy_init();
    std::string body;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return body;
}

static long long extractInt(const std::string &s, const std::string &key) {
    std::string k = "\"" + key + "\"";
    auto p = s.find(k);
    if (p == std::string::npos) return 0;
    p += k.size();
    while (p < s.size() && (s[p] == ' ' || s[p] == ':')) p++;
    if (s.substr(p, 4) == "null") return 0;
    long long v = 0;
    while (p < s.size() && s[p] >= '0' && s[p] <= '9') {
        v = v * 10 + (s[p] - '0');
        p++;
    }
    return v;
}

static std::unordered_map<std::string, std::string> parseObject(const std::string &obj) {
    std::unordered_map<std::string, std::string> result;
    size_t i = 0;
    while (i < obj.size() && obj[i] != '{') i++;
    i++;

    while (i < obj.size()) {
        while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t' || obj[i] == '\n' || obj[i] == '\r' || obj[i] == ',')) {
            i++;
        }
        if (i >= obj.size() || obj[i] == '}') break;

        if (obj[i] != '"') {
            i++;
            continue;
        }
        i++; // skip opening
        std::string key;
        while (i < obj.size() && obj[i] != '"') {
            if (obj[i] == '\\' && i + 1 < obj.size()) {
                key += obj[i + 1];
                i += 2;
            } else {
                key += obj[i];
                i++;
            }
        }
        if (i < obj.size()) i++; // skip closing 

        while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t' || obj[i] == '\n' || obj[i] == '\r' || obj[i] == ':')) {
            i++;
        }

        if (i >= obj.size()) break;

        std::string val;
        if (obj[i] == '"') {
            i++; // skip opening
            while (i < obj.size() && obj[i] != '"') {
                if (obj[i] == '\\' && i + 1 < obj.size()) {
                    val += obj[i + 1];
                    i += 2;
                } else {
                    val += obj[i];
                    i++;
                }
            }
            if (i < obj.size()) i++; // skip closing 
        } else {
            // Read until comma, closing brace or whitespace
            while (i < obj.size() && obj[i] != ',' && obj[i] != '}' && obj[i] != ' ' && obj[i] != '\t' && obj[i] != '\n' && obj[i] != '\r') {
                val += obj[i];
                i++;
            }
        }
        result[key] = val;
    }
    return result;
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
    bool inString = false;

    while (i < json.size()) {
        if (json[i] == '"') {
            int backslashes = 0;
            int j = i - 1;
            while (j >= 0 && json[j] == '\\') {
                backslashes++;
                j--;
            }
            if (backslashes % 2 == 0) {
                inString = !inString;
            }
        }

        if (!inString) {
            if (json[i] == '{') {
                if (depth == 0) objStart = i;
                depth++;
            } else if (json[i] == '}') {
                depth--;
                if (depth == 0 && objStart != std::string::npos) {
                    std::string obj = json.substr(objStart, i - objStart + 1);
                    auto fields = parseObject(obj);
                    
                    std::string name;
                    bool hasName = false;

                    if (fields.count("title") && fields["title"] != "null" && !fields["title"].empty()) {
                        name = fields["title"];
                        hasName = true;
                    } else if (fields.count("story_title") && fields["story_title"] != "null" && !fields["story_title"].empty()) {
                        name = fields["story_title"];
                        hasName = true;
                    }

                    if (hasName) {
                        long long comments = 0;
                        if (fields.count("num_comments") && fields["num_comments"] != "null") {
                            long long parsed_comments = 0;
                            std::string c_str = fields["num_comments"];
                            size_t c_idx = 0;
                            while (c_idx < c_str.size() && c_str[c_idx] >= '0' && c_str[c_idx] <= '9') {
                                parsed_comments = parsed_comments * 10 + (c_str[c_idx] - '0');
                                c_idx++;
                            }
                            comments = parsed_comments;
                        }
                        result.push_back({name, comments});
                    }
                    objStart = std::string::npos;
                }
            } else if (json[i] == ']' && depth == 0) {
                break;
            }
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

