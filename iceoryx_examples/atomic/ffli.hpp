#ifndef ATOMIC_TEST_HPP
#define ATOMIC_TEST_HPP

#include <atomic>

constexpr uint32_t NODE_ALIGNMENT{8};
constexpr uint32_t NODE_SIZE{8};

class MpmcLoFFLi
{
  public:
    using Index_t = uint32_t;

  private:
    struct alignas(NODE_ALIGNMENT) Node
    {
        Index_t indexToNextFreeIndex;
        uint32_t abaCounter;
    };

    static_assert(sizeof(Node) <= NODE_SIZE,
                  "The size of 'Node' must not exceed 8 bytes in order to be lock-free on 64 bit systems!");

    /// @todo iox-#680 introduce typesafe indices with the properties listed below
    ///       id is required that not two loefflis with the same properties
    ///       mix up the id
    ///       value = index
    /// @code
    ///    class Id_t
    ///    {
    ///        Id_t() = default;
    ///        Id_t(const Id_t&) = delete;
    ///        Id_t(Id_t&&) = default;
    ///        ~Id_t() = default;

    ///        Id_t& operator=(const Id_t&) = delete;
    ///        Id_t& operator=(Id_t&) = default;

    ///        friend class MpmcLoFFLi;

    ///      private:
    ///        uint32_t value;
    ///        uint32_t id = MyLoeffliObject.id; //imaginary code
    ///    };
    /// @endcode

    uint32_t m_size{5U};
    Index_t m_invalidIndex{6U};
    std::atomic<Node> m_head{{0U, 1U}};
    Index_t m_nextFreeIndex[5];

  public:
    MpmcLoFFLi() noexcept = default;
    /// @todo iox-#680 move 'init()' to the ctor, remove !m_nextfreeIndex checks

    /// Initializes the lock-free free-list
    /// @param [in] freeIndicesMemory pointer to a memory with the capacity calculated by requiredMemorySize()
    /// @param [in] capacity is the number of elements of the free-list; must be the same used at requiredMemorySize()
    void init(Index_t* freeIndicesMemory, const uint32_t capacity, bool reset) noexcept;

    /// Pop a value from the free-list
    /// @param [out] index for an element to use
    /// @return true if index is valid, false otherwise
    bool pop(Index_t& index) noexcept;

    /// Push previously poped element
    /// @param [in] index to previously poped element
    /// @return true if index is valid or not yet pushed, false otherwise
    bool push(const Index_t index) noexcept;

    void printInfo() noexcept;

};

#endif //ATOMIC_TEST_HPP
