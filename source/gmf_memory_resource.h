#pragma once

#include <memory_resource>
#include <optional>
#include <set>

/**
 * \brief user allocated memory block type
 */
struct entity {
	void* front; // address of first byte (inclusive)
	void* back; // address of last byte (exclusive)

	entity() = delete;
	
	entity(const entity& e) noexcept = default;
	
	entity(entity&& e) noexcept {
		this->front = e.front;
		this->back = e.back;
		e.front = e.back = nullptr;
	}
	
	entity& operator=(entity&& e) noexcept {
		this->front = e.front;
		this->back = e.back;
		e.front = e.back = nullptr;
		return *this;
	}
	
	entity& operator=(const entity& e) noexcept = delete;
	
	entity(void* f, void* b) : front(f), back(b) {
		if (reinterpret_cast<size_t>(f) > reinterpret_cast<size_t>(b)) {
			throw std::invalid_argument("front pointer have less address then back pointer");
		}
	}
	
	/**
	 * \return bytes count in entity
	 */
	[[nodiscard]] virtual size_t get_bytes_count() const noexcept {
		return reinterpret_cast<size_t>(back) - reinterpret_cast<size_t>(front);
	}

	/**
	 * \brief determine whether a given entity is at close left
	 * \param e any other entity
	 * \return true, if given entity is at close left
	 */
	[[nodiscard]] bool close_left(const entity& e) const noexcept {
		return this->front == e.back;
	}

	/**
	 * \brief determine whether a given entity is at close right
	 * \param e any other entity
	 * \return true, if given entity is at close right
	 */
	[[nodiscard]] bool close_right(const entity& e) const noexcept {
		return this->back == e.front;
	}

	virtual ~entity() = default;
};

/**
 * \brief non-existent entity, but on the basis of which work is performed with possibly non-existent entities 
 */
struct imaginary_entity final : entity {
private:
	size_t count_;
public:
	imaginary_entity(void* front) noexcept : entity(front, front), count_(0)
	{}
	
	imaginary_entity(size_t count) noexcept : entity(nullptr, nullptr), count_(count)
	{}

	[[nodiscard]] size_t get_bytes_count() const noexcept override  {
		return this->count_;
	}
};

/**
 * \brief general minimal-fragmentation memory resource
 */
class gmf_memory_resource final : public std::pmr::memory_resource {
public:
	gmf_memory_resource() = delete;
	gmf_memory_resource(const gmf_memory_resource&) = delete;
	gmf_memory_resource(gmf_memory_resource&& mr) noexcept {
		this->memory_pool_start_ = mr.memory_pool_start_;
		this->memory_pool_end_ = mr.memory_pool_end_;

		this->free_entity_pool_by_front_ = std::move(mr.free_entity_pool_by_front_);
		this->free_entity_pool_by_bytes_ = std::move(mr.free_entity_pool_by_bytes_);
		this->occupied_entity_pool_by_front_ = std::move(mr.occupied_entity_pool_by_front_);

		mr.memory_pool_start_ = nullptr;
		mr.memory_pool_end_ = nullptr;
		mr.free_entity_pool_by_front_.clear();
		mr.free_entity_pool_by_bytes_.clear();
		mr.occupied_entity_pool_by_front_.clear();
	}

	gmf_memory_resource& operator=(gmf_memory_resource&& mr) noexcept {
		this->memory_pool_start_ = mr.memory_pool_start_;
		this->memory_pool_end_ = mr.memory_pool_end_;

		this->free_entity_pool_by_front_ = std::move(mr.free_entity_pool_by_front_);
		this->free_entity_pool_by_bytes_ = std::move(mr.free_entity_pool_by_bytes_);
		this->occupied_entity_pool_by_front_ = std::move(mr.occupied_entity_pool_by_front_);

		mr.memory_pool_start_ = nullptr;
		mr.memory_pool_end_ = nullptr;
		mr.free_entity_pool_by_front_.clear();
		mr.free_entity_pool_by_bytes_.clear();
		mr.occupied_entity_pool_by_front_.clear();

		return *this;
	}
	gmf_memory_resource& operator=(const gmf_memory_resource& e) noexcept = delete;

	gmf_memory_resource(void* memory_pool_start, void* memory_pool_start_end) : memory_pool_start_(memory_pool_start), memory_pool_end_(memory_pool_start_end) {
		if (memory_pool_start == nullptr || memory_pool_start_end == nullptr) {
			throw std::invalid_argument("memory pool is not valid");
		}

		free_entity_pool_by_bytes_.insert(entity{ memory_pool_start_, memory_pool_end_ });
		free_entity_pool_by_front_.insert(entity{ memory_pool_start_, memory_pool_end_ });
	}
	~gmf_memory_resource() noexcept override = default;
private:
	void* do_allocate(size_t _Bytes, [[maybe_unused]] size_t _Align) override {
		if (_Bytes == 0) {
			return nullptr;
		}

		auto free_entity_it = this->free_entity_pool_by_bytes_.lower_bound(imaginary_entity { _Bytes });

		if (free_entity_it == this->free_entity_pool_by_bytes_.end()) {
			return nullptr;
		}

		const entity ideal_free_entity = *free_entity_it;
		const entity occupied_entity = { ideal_free_entity.front, offset_void_ptr(ideal_free_entity.front, _Bytes) };

		this->free_entity_pool_by_bytes_.erase(free_entity_it);
		this->free_entity_pool_by_front_.erase(ideal_free_entity);

		this->occupied_entity_pool_by_front_.insert(occupied_entity);

		if (ideal_free_entity.get_bytes_count() > _Bytes) {
			const entity new_entity = entity { occupied_entity.back, ideal_free_entity.back };

			this->free_entity_pool_by_bytes_.insert(new_entity);
			this->free_entity_pool_by_front_.insert(new_entity);
		}

		return occupied_entity.front;
	}
	void do_deallocate(void* _Ptr, [[maybe_unused]] size_t _Bytes, [[maybe_unused]] size_t _Align) override {
		auto occupied_entity_it = this->occupied_entity_pool_by_front_.lower_bound(imaginary_entity { _Ptr });

		if (occupied_entity_it == this->occupied_entity_pool_by_front_.end() || occupied_entity_it->front != _Ptr) {
			return;
		}

		auto get_preview_free = [_Ptr, this]() {
			auto free_entity_it = this->free_entity_pool_by_front_.lower_bound(imaginary_entity { _Ptr });

			if (free_entity_it != this->free_entity_pool_by_front_.begin()) {
				--free_entity_it;
			}

			if (free_entity_it == this->free_entity_pool_by_front_.end()) {
				return std::optional<entity>();
			}

			if (free_entity_it->front > _Ptr) {
				return std::optional<entity>();
			}

			return std::optional<entity>(*free_entity_it);
		};
		auto get_next_free = [_Ptr, this]() {
			auto free_entity_it = this->free_entity_pool_by_front_.upper_bound(imaginary_entity{ _Ptr });

			if (free_entity_it == this->free_entity_pool_by_front_.end()) {
				return std::optional<entity>();
			}

			return std::optional<entity>(*free_entity_it);
		};

		std::optional<entity> left_free = get_preview_free();
		std::optional<entity> right_free = get_next_free();

		entity new_free_entity = *occupied_entity_it;

		if (left_free.has_value() && new_free_entity.close_left(left_free.value())) {
			new_free_entity.front = left_free.value().front;
			this->free_entity_pool_by_front_.erase(left_free.value());
			this->free_entity_pool_by_bytes_.erase(this->free_entity_pool_by_bytes_.lower_bound(left_free.value()));
		}

		if (right_free.has_value() && new_free_entity.close_right(right_free.value())) {
			new_free_entity.back = right_free.value().back;
			this->free_entity_pool_by_front_.erase(right_free.value());
			this->free_entity_pool_by_bytes_.erase(this->free_entity_pool_by_bytes_.lower_bound(right_free.value()));
		}

		this->occupied_entity_pool_by_front_.erase(occupied_entity_it);

		this->free_entity_pool_by_front_.insert(new_free_entity);
		this->free_entity_pool_by_bytes_.insert(new_free_entity);
	}
	[[nodiscard]] bool do_is_equal([[maybe_unused]] const memory_resource& _That) const noexcept override {
		return false;
	}

	/**
	 * \param ptr 
	 * \param bytes 
	 * \return pointer shifted by a given number of bytes 
	 */
	static void* offset_void_ptr(void* ptr, size_t bytes) {
		return reinterpret_cast<void*>(reinterpret_cast<size_t>(ptr) + bytes);
	}

	/**
	 * \brief entity ordering by bytes func
	 */
	inline static auto entity_ordering_by_bytes_ = [](const entity& l, const entity& r) {
		return l.get_bytes_count() < r.get_bytes_count();
	};
	
	/**
	 * \brief entity ordering by front address func
	 */
	inline static auto entity_ordering_by_front_address__ = [](const entity& l, const entity& r) {
		return l.front < r.front;
	};
	
	/**
	 * \brief set of free entities ordering by byte count
	 */
	std::multiset<entity, decltype(entity_ordering_by_bytes_)> free_entity_pool_by_bytes_;
	/**
	 * \brief set of free entities ordering by front address
	 */
	std::set<entity, decltype(entity_ordering_by_front_address__)> free_entity_pool_by_front_;
	/**
	 * \brief set of user-allocated entities ordering by front address
	 */
	std::set<entity, decltype(entity_ordering_by_front_address__)> occupied_entity_pool_by_front_;

	void* memory_pool_start_; // address to first byte (inclusive)
	void* memory_pool_end_; // address of last byte (exclusive)
};
