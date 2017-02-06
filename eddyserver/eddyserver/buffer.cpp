#include "buffer.h"

namespace eddyserver
{
    Buffer::Buffer()
        : reader_pos_(0)
        , writer_pos_(0)
        , is_dynmic_(false)
    {
    }

    Buffer::Buffer(size_t size)
        : reader_pos_(0)
        , writer_pos_(0)
        , is_dynmic_(false)
    {
        ensure_writable_bytes(size);
    }

    Buffer::Buffer(const char *data, size_t size)
        : reader_pos_(0)
        , writer_pos_(0)
        , is_dynmic_(false)
    {
        write(data, size);
    }

    Buffer::Buffer(const Buffer &other)
        : is_dynmic_(other.is_dynmic())
        , reader_pos_(other.reader_pos_)
        , writer_pos_(other.writer_pos_)
    {
        if (!other.is_dynmic())
        {
            memcpy(static_data_, other.static_data_, kDynamicThreshold);
        }
        else
        {
            dynamic_data_ = std::make_unique<DynamicVector>();
            *dynamic_data_ = *other.dynamic_data_;
        }
    }

    Buffer::Buffer(Buffer &&other)
        : is_dynmic_(other.is_dynmic_)
        , reader_pos_(other.reader_pos_)
        , writer_pos_(other.writer_pos_)
    {
        if (!other.is_dynmic())
        {
            memcpy(static_data_, other.static_data_, kDynamicThreshold);
        }
        else
        {
            dynamic_data_ = std::move(other.dynamic_data_);
        }

        other.reader_pos_ = 0;
        other.writer_pos_ = 0;
        other.is_dynmic_ = false;
    }

    Buffer& Buffer::operator= (Buffer &&rhs)
    {
        if (std::addressof(rhs) != this)
        {
            is_dynmic_ = rhs.is_dynmic();
            reader_pos_ = rhs.reader_pos_;
            writer_pos_ = rhs.writer_pos_;
            if (!rhs.is_dynmic())
            {
                memcpy(static_data_, rhs.static_data_, kDynamicThreshold);
            }
            else
            {
                dynamic_data_ = std::move(rhs.dynamic_data_);
            }

            rhs.reader_pos_ = 0;
            rhs.writer_pos_ = 0;
            rhs.is_dynmic_ = false;
        }
        return *this;
    }

    Buffer& Buffer::operator= (const Buffer &rhs)
    {
        if (std::addressof(rhs) != this)
        {
            is_dynmic_ = rhs.is_dynmic();
            reader_pos_ = rhs.reader_pos_;
            writer_pos_ = rhs.writer_pos_;
            if (!rhs.is_dynmic())
            {
                memcpy(static_data_, rhs.static_data_, kDynamicThreshold);
            }
            else
            {          
                if (dynamic_data_ == nullptr)
                {
                    dynamic_data_ = std::make_unique<DynamicVector>();
                }
                *dynamic_data_ = *rhs.dynamic_data_;
            }
        }
        return *this;
    }

    // 交换数据
    void Buffer::swap(Buffer &other)
    {
        if (std::addressof(other) != this)
        {
            dynamic_data_.swap(other.dynamic_data_);
            std::swap(is_dynmic_, other.is_dynmic_);
            std::swap(reader_pos_, other.reader_pos_);
            std::swap(writer_pos_, other.writer_pos_);
            std::swap(static_data_, other.static_data_);
        }
    }

    // 清空
    void Buffer::clear()
    {
        reader_pos_ = 0;
        writer_pos_ = 0;
        if (is_dynmic())
        {
            dynamic_data_->clear();
        }
    }

    // 设为动态数组
    void Buffer::set_dynamic()
    {
        assert(!is_dynmic());
        if (!is_dynmic())
        {
            is_dynmic_ = true;
            const size_t content_size = readable();
            dynamic_data_ = std::make_unique<DynamicVector>(content_size);
            dynamic_data_->insert(dynamic_data_->begin(), static_data_ + reader_pos_, static_data_ + writer_pos_);
            reader_pos_ = 0;
            writer_pos_ = content_size;
            assert(content_size == readable());
        }
    }

    // 设置容量大小
    void Buffer::reserve(size_t size)
    {
        if (!is_dynmic())
        {
            if (size <= kDynamicThreshold)
            {
                return;
            }
            set_dynamic();
        }
        dynamic_data_->reserve(size);
    }

    // 获取全部
    void Buffer::retrieve_all()
    {
        reader_pos_ = 0;
        writer_pos_ = 0;
    }

    // 获取数据
    void Buffer::retrieve(size_t size)
    {
        assert(readable() >= size);
        if (readable() > size)
        {
            reader_pos_ += size;
        }
        else
        {
            retrieve_all();
        }
    }

    // 写入数据大小
    void Buffer::has_written(size_t size)
    {
        assert(writeable() >= size);
        writer_pos_ += size;
    }

    // 分配空间
    void Buffer::make_space(size_t size)
    {
        if (writeable() + prependable() < size)
        {
            if (!is_dynmic())
            {
                set_dynamic();
            }
            dynamic_data_->resize(writer_pos_ + size);
        }
        else
        {
            if (!is_dynmic())
            {
                memmove(static_data_, static_data_ + reader_pos_, writer_pos_ - reader_pos_);
            }
            else
            {
                memmove(dynamic_data_->data(), dynamic_data_->data() + reader_pos_, writer_pos_ - reader_pos_);
            }

            const size_t readable_size = readable();
            reader_pos_ = 0;
            writer_pos_ = readable_size;
            assert(readable_size == readable());
        }
    }

    // 确保可写字节
    void Buffer::ensure_writable_bytes(size_t size)
    {
        if (writeable() < size)
        {
            make_space(size);
        }
        assert(writeable() >= size);
    }

    // 读取字符串
    std::string Buffer::read_string()
    {
        assert(readable() > 0);
        const uint8_t *eos = data();
        while (*eos++);
        size_t lenght = eos - data() - 1;
        assert(readable() >= lenght);
        std::string value;
        if (lenght > 0)
        {
            value.resize(lenght);
            memcpy(const_cast<char*>(value.data()), data(), lenght);
            retrieve(lenght);
        }
        return value;
    }

    void Buffer::read_string(std::string *out_value)
    {
        assert(readable() > 0);
        const uint8_t *eos = data();
        while (*eos++);
        size_t lenght = eos - data() - 1;
        assert(readable() >= lenght);

        out_value->clear();
        if (lenght > 0)
        {
            out_value->resize(lenght);
            memcpy(const_cast<char*>(out_value->data()), data(), lenght);
            retrieve(lenght);
        }
    }

    // 读取长度和字符串
    std::string Buffer::read_lenght_and_string()
    {
        assert(readable() >= sizeof(uint32_t));
        uint32_t lenght = 0;
        memcpy(&lenght, data(), sizeof(uint32_t));
        retrieve(sizeof(uint32_t));
        std::string value;
        if (lenght > 0)
        {
            value.resize(lenght);
            memcpy(const_cast<char*>(value.data()), data(), lenght);
            retrieve(lenght);
        }
        return value;
    }

    void Buffer::read_lenght_and_string(std::string *out_value)
    {
        assert(readable() >= sizeof(uint32_t));
        uint32_t lenght = 0;
        memcpy(&lenght, data(), sizeof(uint32_t));
        retrieve(sizeof(uint32_t));

        out_value->clear();
        if (lenght > 0)
        {
            out_value->resize(lenght);
            memcpy(const_cast<char*>(out_value->data()), data(), lenght);
            retrieve(lenght);
        }
    }

    // 写入数据
    size_t Buffer::write(const void *data, size_t size)
    {
        ensure_writable_bytes(size);
        if (!is_dynmic())
        {
            memcpy(static_data_ + writer_pos_, data, size);
        }
        else
        {
            dynamic_data_->insert(dynamic_data_->begin() + writer_pos_,
                reinterpret_cast<const uint8_t*>(data),
                reinterpret_cast<const uint8_t*>(data)+size);
        }
        has_written(size);
        return size;
    }

    // 写入字符串
    void Buffer::write_string(const std::string &value)
    {
        if (!value.empty())
        {
            write(const_cast<char*>(value.data()), value.size());
        }
    }

    // 写入长度和字符串
    void Buffer::write_lenght_and_string(const std::string &value)
    {
        write_pod<uint32_t>(value.size());
        write_string(value);
    }
}
