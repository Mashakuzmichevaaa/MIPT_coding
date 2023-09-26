#include <iostream>
#include <iterator>
#include <memory>

template<size_t N>
class StackStorage {
  private:
  void* free_memory_;
  size_t left_bytes_;

  public:
  StackStorage() {
    free_memory_ = malloc(N);
    left_bytes_ = N;
  }

  StackStorage(const StackStorage<N>& storage) = delete;

  template<typename T>
  void* allocate(size_t num) {
    if (std::align(alignof(T[num]),
                   sizeof(T[num]),
                   free_memory_,
                   left_bytes_)) {
      void* result = free_memory_;
      free_memory_ = static_cast<char*>(free_memory_) + sizeof(T[num]);
      left_bytes_ -= sizeof(T[num]);
      return result;
    }
    throw std::bad_alloc();
  }
};

template<typename T, size_t N>
class StackAllocator {
  private:
  StackStorage<N>* storage_;

  public:
  using value_type = T;

  StackAllocator(StackStorage<N>& storage) {
    storage_ = &storage;
  }

  template<typename U>
  StackAllocator(const StackAllocator<U, N>& alloc) {
    storage_ = alloc.get_storage();
  }

  template <typename A>
  StackAllocator &operator=(const StackAllocator<A, N> &other) {
    storage_ = other.get_storage();
    return *this;
  }

  value_type* allocate(size_t num) {
    value_type* result =
            reinterpret_cast<value_type*>(storage_->template allocate<value_type>(num));
    return result;
  }

  template<typename U>
  struct rebind {
    using other = StackAllocator<U, N>;
  };

  void deallocate(value_type*, size_t) {}

  StackStorage<N>* get_storage() const {
    return storage_;
  }

  ~StackAllocator() {}
};

template <typename T, typename A = std::allocator<T>>
class List {
  private:
  struct Node {
    T value;
    Node* to = nullptr;
    Node* from = nullptr;

    Node() = default;
    Node(const T& value) : value(value) {}
    Node& operator*() { return *this; }
  };

  using node_alloc = typename std::allocator_traits<A>::template rebind_alloc<Node>;
  using alloc_traits = std::allocator_traits<node_alloc>;
  node_alloc alloc_;
  size_t size_;
  Node* empty_node_;

  void construct_empty_node() {
    empty_node_ = alloc_.allocate(1);
    empty_node_->from = empty_node_;
    empty_node_->to = empty_node_;
  }

  void construct_memory(const List<T, A>& other) {
    size_ = 0;
    construct_empty_node();
    Node* tmp = empty_node_;
    try {
      for (auto i = other.begin(); i != other.end(); ++i) {
        try {
          tmp->to = alloc_.allocate(1);
          alloc_traits::construct(alloc_, tmp->to, *i);
        } catch (...) {
          throw;
        }
        tmp->to->from = tmp;
        tmp = tmp->to;
        ++size_;
      }
      tmp->to = empty_node_;
      empty_node_->from = tmp;
    } catch (...) {
      tmp = empty_node_->to;
      Node* node = tmp->to;
      for (size_t i = 0; i < size_; ++i) {
        alloc_traits::deallocate(alloc_, tmp, 1);
        tmp = node;
        node = tmp->to;
      }
      size_ = 0;
      empty_node_->from = empty_node_;
      empty_node_->to = empty_node_;
    }
  }

  public:
  node_alloc& get_allocator() { return alloc_; }

  size_t size() const { return size_; }

  List() : alloc_(node_alloc()), size_(0) {
    construct_empty_node();
  }

  List(size_t n, const T& value, A alloc) : alloc_(alloc), size_(0) {
    construct_empty_node();
    while (size_ < n) push_front(value);
  }

  List(size_t n) {
    size_ = 0;
    construct_empty_node();
    Node* tmp = empty_node_;
    Node* node;
    for (size_t i = 0; i < n; ++i) {
      try {
        node = alloc_.allocate(1);
      } catch (...) {
        throw;
      }
      try {
        alloc_traits::construct(alloc_, node);
      } catch (...) {
        tmp = empty_node_->to;
        for (size_t j = 0; j < i; ++j) {
          node = tmp->to;
          alloc_traits::deallocate(alloc_, tmp, 1);
          tmp = node;
        }
        empty_node_->from = empty_node_;
        empty_node_->to = empty_node_;
        return;
      }
      node->from = tmp;
      tmp->to = node;
      tmp = node;
    }
    size_ = n;
    tmp->to = empty_node_;
    empty_node_->from = tmp;
  }

  List(size_t n, const T& value) : size_(0) {
    construct_empty_node();
    while (size_ < n) push_front(value);
  }

  List(size_t n, const A alloc) : alloc_(alloc) {
    size_ = 0;
    construct_empty_node();
    Node* tmp = empty_node_;
    while (size_ < n) {
      tmp->to = alloc_.allocate(1);
      alloc_traits::construct(alloc_, tmp->to);
      tmp->to->from = tmp;
      tmp = tmp->to;
      size_++;
    }
    empty_node_->from = tmp;
    tmp->to = empty_node_;
  }

  List(const A alloc) : alloc_(alloc_traits::select_on_container_copy_construction(alloc)) {
    size_ = 0;
    construct_empty_node();
  }

  List(const List<T, A>& other)
      : alloc_(alloc_traits::select_on_container_copy_construction(other.alloc_)) {
    size_ = other.size();
    construct_memory(other);
  }

  List& operator=(const List<T, A>& other) {
    while (size_ > 0) erase(cbegin());
    if (alloc_traits::propagate_on_container_copy_assignment::value) {
      alloc_ = other.alloc_;
    }
    construct_memory(other);
    return *this;
  }

  ~List() {
    while (size_ > 0) {
      erase(cbegin());
    }
  }

  template <bool is_const>
  struct Iterator {
  private:
    Node* node_;

  public:
    using value_type = std::conditional_t<is_const, const T, T>;
    using reference = std::conditional_t<is_const, const T&, T&>;
    using pointer = std::conditional_t<is_const, const T*, T*>;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = long long;

    Iterator() = default;

    Iterator(const Iterator<is_const> &iter) {
      node_ = iter.get_node_ptr();
    }

    Iterator(Node* node) {
      node_ = node;
    }

    Iterator<is_const> &operator=(const Iterator<is_const> &iter) {
      node_ = iter.get_node_ptr();
      return *this;
    }

    reference operator*() { return node_->value; }

    pointer operator->() { return &(operator*()); }

    Iterator<is_const> &operator++() {
      node_ = node_->to;
      return *this;
    }

    Iterator<is_const> operator++(int) {
      Iterator<is_const> tmp(*this);
      ++(*this);
      return tmp;
    }

    Iterator<is_const> &operator--() {
      node_ = node_->from;
      return *this;
    }

    Iterator<is_const> operator--(int) {
      Iterator<is_const> tmp(*this);
      --(*this);
      return tmp;
    }

    bool operator==(const Iterator& iter) const {
      return node_ == iter.get_node_ptr();
    }

    bool operator!=(const Iterator& iter) const {
      return node_ != iter.get_node_ptr();
    }

    operator Iterator<true>() const {
      Iterator<true> const_iter(node_);
      return const_iter;
    }

    Node* get_node_ptr() const { return node_; }
  };

  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() noexcept {
    return iterator(empty_node_->to);
  }

  const_iterator begin() const noexcept {
    return cbegin();
  }

  const_iterator cbegin() const noexcept {
    return const_iterator(empty_node_->to);
  }

  reverse_iterator rbegin() noexcept {
    return std::make_reverse_iterator(end());
  }

  const_reverse_iterator crbegin() const noexcept {
    return std::make_reverse_iterator(cend());
  }

  const_reverse_iterator rbegin() const noexcept {
    return crbegin();
  }

  iterator end() noexcept {
    return iterator(empty_node_);
  }

  const_iterator end() const noexcept {
    return cend();
  }

  const_iterator cend() const noexcept {
    return const_iterator(empty_node_);
  }

  reverse_iterator rend() noexcept {
    return std::make_reverse_iterator(begin());
  }

  const_reverse_iterator crend() const noexcept {
    return std::make_reverse_iterator(cbegin());
  }

  const_reverse_iterator rend() const noexcept {
    return crend();
  }

  void insert(const_iterator iter, const T& value) {
    Node* node = iter.get_node_ptr();
    Node* new_node = alloc_traits::allocate(alloc_, 1);
    new_node->value = value;
    new_node->to = node;
    new_node->from = node->from;
    node->from->to = new_node;
    node->from = new_node;
    ++size_;
  }

  void erase(const_iterator iter) {
    Node* tmp = iter.get_node_ptr();
    tmp->to->from = tmp->from;
    tmp->from->to = tmp->to;
    alloc_traits::destroy(alloc_, tmp);
    alloc_traits::deallocate(alloc_, tmp, 1);
    --size_;
  }

  void push_back(const T &value) {
    insert(end(), value);
  }

  void push_front(const T &value) {
    insert(begin(), value);
  }

  void pop_back() {
    erase(--cend());
  }

  void pop_front() {
    erase(cbegin());
  }
};
