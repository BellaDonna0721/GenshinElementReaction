#pragma once
#include<vector>
#include <functional>

template<class K, class V, class Hash = std::hash<K>>
class myUnorderedMap{
private:

    size_t cur_size;
    std::vector<std::vector<std::pair<K, V>>> buckets;
    static constexpr int DEFAULT_BUCKETS_SIZE = 8;
    static constexpr float MAX_FACTOR = 0.75f;

    bool reHash(){
        size_t new_size = 2*buckets.size();
        std::vector<std::vector<std::pair<K, V>>> newBuckets(new_size);
        Hash hasher;
        for (auto& bucket : buckets) {
            for (auto& pair : bucket) {
                size_t new_idx = hasher(pair.first) & (new_size - 1);
                newBuckets[new_idx].push_back(std::move(pair));
            }
        }
        buckets.swap(newBuckets);
        return true;
    }

    // 非 const 版本，允许修改 value
    std::pair<K, V>* find_not_const(const K& key) {
        Hash hasher;
        size_t bucket_index = hasher(key) & (buckets.size() - 1);
        for (std::pair<K, V>& p : buckets[bucket_index]){
            if (p.first == key)
            return &p;
        }
        return nullptr;
    }
public:
    myUnorderedMap(){
        buckets = std::vector<std::vector<std::pair<K, V>>>(DEFAULT_BUCKETS_SIZE);
        cur_size = 0;
    }

    ~myUnorderedMap() {

    }

    //拷贝构造
    myUnorderedMap(const myUnorderedMap& other): cur_size(other.cur_size), buckets(other.buckets){}

    void swap(myUnorderedMap& other) noexcept {
        std::swap(buckets, other.buckets);
        std::swap(cur_size, other.cur_size);
    }
    //拷贝赋值
    myUnorderedMap& operator=(const myUnorderedMap& other){
        if (this != &other) {
            myUnorderedMap temp(other); 
            swap(temp);                 
        }
        return *this;
    }

    const std::pair<K, V>* find(const K& key) const{
        //桶数是2的幂次 使用位运算算hash更快
        Hash hasher;
        size_t bucket_index = hasher(key) & (buckets.size() - 1);
        for (const std::pair<K, V>& p : buckets[bucket_index]){
            if (p.first == key)
            return &p;
        }
        return nullptr;
    }


    //若不存在则插入 若存在则更新值
    bool insert_or_assign(const K& key, const V& value){
        auto f = find_not_const(key);
        if (f){
            f->second = value;
            return true;
        }
        else{
            if (((cur_size+1)*1.0)/buckets.size() > MAX_FACTOR){
                reHash(); 
            } 
        
            Hash hasher;
            size_t bucket_index = hasher(key) & (buckets.size()-1);
            buckets[bucket_index].push_back(std::make_pair(key, value));
            cur_size++;
            return true;
        }
    }

    bool erase(const K& key) {
        Hash hasher;
        size_t bucket_index = hasher(key) & (buckets.size() - 1);
        for (size_t i=0; i<buckets[bucket_index].size(); i++){
            if (buckets[bucket_index][i].first == key){
                buckets[bucket_index][i] = std::move(buckets[bucket_index].back());
                buckets[bucket_index].pop_back();
                cur_size--; 
                return true;
            }
        }
        return false;
    }

    size_t size() const{
        return cur_size;
    }
};