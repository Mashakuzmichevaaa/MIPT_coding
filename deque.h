#include <vector>

template <typename T>
class Deque {
private:
  static const int kBatchSize = 32;
  std::vector<char*> buffer_;
  std::vector<T*> batches_;
  size_t begin_ = 0;
  size_t end_ = 0;

  size_t Realloc(std::vector<T*>& first_batches, const std::vector<T*>& second_batches) {
    size_t tmp = second_batches.size() / 2 + 1;
    first_batches.resize(second_batches.size() + 2 * tmp, nullptr);
    buffer_.push_back(nullptr);
    T* ptr = reinterpret_cast<T*>(buffer_.back() = new char[2 * tmp * kBatchSize * sizeof(T)]);
    for (size_t i = 0; i < tmp; ++i, ptr += kBatchSize) {
      first_batches[i] = ptr;
    }
    for (size_t i = tmp + second_batches.size(); i < first_batches.size(); ++i, ptr += kBatchSize) {
      first_batches[i] = ptr;
    }
    return tmp;
  }

  explicit Deque(size_t size) {
    batches_ = std::vector<T*> (size / kBatchSize + 1);
    buffer_.push_back(nullptr);
    T* ptr = reinterpret_cast<T*>(buffer_.back() = new char[batches_.size() * kBatchSize * sizeof(T)]);
    for (auto& it : batches_) {
      it = ptr;
      ptr += kBatchSize;
    }
  }

public:
  Deque() {}

  Deque(int size) : Deque(static_cast<size_t>(size)) {
    for (int i = 0; i < size; ++i) {
      new(end().operator->()) T();
      ++end_;
    }
  }

  Deque(size_t size, const T& value) : Deque(size) {
    for (int i = 0; i < size; ++i) {
      new ((end() + i).operator->()) T(value);
    }
    end_ = size;
  }

  Deque(const Deque& deque) : Deque(deque.size()) {
    for (int i = end_; i < deque.size(); ++i) {
      new ((end() + i).operator->()) T(deque[i]);
    }
    end_ = deque.size();
  }

  Deque& operator=(const Deque& deq) {
    Deque copy = deq;
    std::swap(batches_, copy.batches_);
    std::swap(buffer_, copy.buffer_);
    std::swap(end_, copy.end_);
    std::swap(begin_, copy.begin_);
    return *this;
  }

  size_t size() const { return end_ - begin_; }

  T& operator[](size_t index) { return *(begin() + index); }

  const T& operator[](size_t index) const { return *(cbegin() + index); }

  T& at(size_t index) {
    if (index >= size()) {
      throw std::out_of_range("Index out of range");
    }
    return operator[](index);
  }

  const T& at(size_t index) const {
    if (index >= size()) {
      throw std::out_of_range("Index out of range");
    }
    return operator[](index);
  }

  void push_back(const T& value) {
    if (batches_.size() != end_ / kBatchSize) {
      try {
        new (end().operator->()) T(value);
      } catch (...) {
        throw;
      }
    } else {
      std::vector<T*> support;
      size_t tmp = Realloc(support, batches_);
      size_t id = tmp * kBatchSize + end_;
      try {
        new (support[id / kBatchSize] + id % kBatchSize) T(value);
      } catch (...) {
        throw;
      }
      begin_ += tmp * kBatchSize;
      end_ += tmp * kBatchSize;
      std::copy(batches_.begin(), batches_.end(), support.begin() + tmp);
      batches_.swap(support);
    }
    ++end_;
  }

  void push_front(const T& value) {
    if (begin_ > 0) {
      try {
        new ((begin() - 1).operator->()) T(value);
      } catch (...) {
        throw;
      }
    } else {
      std::vector<T*> support;
      size_t tmp = Realloc(support, batches_);
      size_t id = tmp * kBatchSize + begin_ - 1;
      try {
        new (support[id / kBatchSize] + id % kBatchSize) T(value);
      } catch (...) {
        throw;
      }
      begin_ += tmp * kBatchSize;
      end_ += tmp * kBatchSize;
      std::copy(batches_.begin(), batches_.end(), support.begin() + tmp);
      batches_.swap(support);
    }
    --begin_;
  }

  void pop_back() {
    iterator iter = end() - 1;
    T* ptr = iter.operator->();
    ptr->~T();
    --end_;
  }

  void pop_front() {
    iterator iter = begin();
    T* ptr = iter.operator->();
    ptr->~T();
    begin_++;
  }

  template <bool is_const>
  struct base_iterator {
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::conditional_t<is_const, const T, T>;
    using batches_pointer = typename std::conditional<is_const, T* const*, T**>::type;
    using const_iterator = base_iterator<true>;

    batches_pointer batches_ptr_ = nullptr;
    int index_;

    base_iterator(batches_pointer batches_begin, size_t idx_begin) {
      batches_ptr_ = batches_begin + idx_begin / kBatchSize;
      index_ = idx_begin % kBatchSize;
    }

    operator const_iterator() const { return const_iterator(batches_ptr_, index_); }

    base_iterator& operator++() { return (*this += 1); }

    base_iterator operator++(int) {
      base_iterator tmp = *this;
      ++*this;
      return tmp;
    }

    base_iterator& operator--() { return (*this -= 1); }

    base_iterator operator--(int) {
      base_iterator tmp = *this;
      --*this;
      return tmp;
    }

    base_iterator& operator+=(int delta) {
      if (delta > 0) {
        batches_ptr_ += (index_ + delta) / kBatchSize;
        index_ = (index_ + delta) % kBatchSize;
      } else {
        batches_ptr_ += (index_ + delta - kBatchSize + 1) / kBatchSize;
        index_ = (((index_ + delta) % kBatchSize) + kBatchSize) % kBatchSize;
      }
      return *this;
    }

    base_iterator& operator-=(int delta) { return (*this += -delta); }

    base_iterator operator+(int delta) const {
      base_iterator res = *this;
      res += delta;
      return res;
    }

    int operator-(const base_iterator& iter) const {
      return (batches_ptr_ - iter.batches_ptr_) * kBatchSize + (index_ - iter.index_);
    }

    base_iterator operator-(int delta) const { return (*this) + (-delta); }

    value_type& operator*() const { return *(operator->()); }

    value_type* operator->() const {
      return *batches_ptr_ + index_;
    }

    value_type& operator[](int index) const {
      return batches_ptr_[(index + index_) / kBatchSize][(index + index_) % kBatchSize];
    }

    bool operator<(const base_iterator& iter) const {
      return (batches_ptr_ == iter.batches_ptr_) ? index_ < iter.index_ : batches_ptr_ < iter.batches_ptr_;
    }

    bool operator>(const base_iterator& iter) const { return iter < *this; }

    bool operator==(const base_iterator& iter) const { return !(*this < iter || iter < *this); }

    bool operator!=(const base_iterator& iter) const { return !(*this == iter); }

    bool operator<=(const base_iterator& iter) const { return !(iter < *this); }

    bool operator>=(const base_iterator& iter) const { return !(*this < iter); }

  };

  using iterator = base_iterator<false>;
  using const_iterator = base_iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() { return iterator(batches_.data(), begin_); }

  iterator end() { return begin() + size(); }

  const_iterator begin() const { return cbegin(); }

  const_iterator end() const { return cend(); }

  const_iterator cbegin() const { return const_iterator(batches_.data(), begin_); }

  const_iterator cend() const { return cbegin() + size(); }

  reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }

  reverse_iterator rend() { return std::make_reverse_iterator(begin()); }

  const_reverse_iterator crbegin() const { return std::make_reverse_iterator(cend()); }

  const_reverse_iterator crend() const { return std::make_reverse_iterator(cbegin()); }

  void insert(iterator id, const T& value) {
    if (id == end()) {
      push_back(value);
    } else {
      push_back(value);
      for (iterator iter = end() - 1; iter != id; --iter) {
        std::swap(*iter, *(iter - 1));
      }
    }
  }

  void erase(iterator id) {
    for (iterator iter = id; iter != end() - 1; ++iter) {
      std::swap(*iter, *(iter + 1));
    }
    pop_back();
  }

  ~Deque() {
    for (iterator iter = begin(); iter < end(); ++iter) {
      iter->~T();
    }
    while (!buffer_.empty()) {
      delete[] buffer_.back();
      buffer_.pop_back();
    }
  }
};
