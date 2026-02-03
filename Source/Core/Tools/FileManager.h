//
// Created by Orgest on 11/10/2025.
//
#pragma once
#include <cstdio>
#include <span>
#include <utility>
#include "Tools/Vector.h"

#include "../PrimTypes.h"

struct FileManager
{
	struct Handle
	{
		Handle() noexcept = default;
		Handle(const char* path, const char* mode) noexcept { open(path, mode); }

		Handle(const Handle&) = delete;
		Handle& operator=(const Handle&) = delete;

		Handle(Handle&& other) noexcept : mFile(other.mFile)
		{
			other.mFile = nullptr;
		}
		Handle& operator=(Handle&& other) noexcept
		{
			if (this != &other)
			{
				close();
				mFile = other.mFile;
				other.mFile = nullptr;
			}
			return *this;
		}

		~Handle() noexcept { close(); }

		bool open(const char* path, const char* mode) noexcept
		{
		    if (isOpen()) close();
#ifdef _MSC_VER
			fopen_s(&mFile, path, mode);
#else
			mFile = std::fopen(path, mode);
#endif
			return mFile != nullptr;
		}

		void close() noexcept
		{
			if (mFile)
			{
				std::fclose(mFile);
				mFile = nullptr;
			}
		}

		[[nodiscard]] bool isOpen() const noexcept { return mFile != nullptr; }
		explicit operator bool() const noexcept { return isOpen(); }

	    i32 size() const noexcept
		{
		    if (!mFile) return 0;

		    const i32 currentPos = std::ftell(mFile);
		    std::fseek(mFile, 0, SEEK_END);
		    const i32 totalSize = std::ftell(mFile);
		    std::fseek(mFile, currentPos, SEEK_SET);

		    return totalSize;
		}

	    template <typename T>
        [[nodiscard]] Result<void> read(std::span<T> buffer) const noexcept
		{
		    if (buffer.empty()) return {};

		    const size_t readCount = std::fread(buffer.data(), sizeof(T), buffer.size(), mFile);

		    if (readCount != buffer.size())
		    {
		        return std::unexpected(FileReadFailed);
		    }
		    return {};
		}

	    template <typename T>
        [[nodiscard]] Result<void> write(std::span<const T> buffer) const noexcept
		{
		    if (buffer.empty()) return {};

		    const size_t writeCount = std::fwrite(buffer.data(), sizeof(T), buffer.size(), mFile);

		    if (writeCount != buffer.size())
		    {
		        return std::unexpected(FileWriteFailed);
		    }
		    return {};
		}

		size_t read(void* dst, size_t size, size_t count) const noexcept
		{
			return std::fread(dst, size, count, mFile);
		}

		size_t write(const void* src, size_t size, size_t count) const noexcept
		{
			return std::fwrite(src, size, count, mFile);
		}

		void seek(i32 offset, i32 origin) const noexcept { std::fseek(mFile, offset, origin); }
		[[nodiscard]] i32 tell() const noexcept { return std::ftell(mFile); }
		[[nodiscard]] Result<void> rewind() const noexcept
		{
			if (std::fseek(mFile, 0L, SEEK_SET) != 0)
			{
				return std::unexpected(FileReadFailed);
			}
			return {};
		}
	private:
		FILE* mFile = nullptr;
	};

	// File I/O (No Allocation)
	/// Returns file size if readable
	static Result<i32> GetFileSize(const char* path) noexcept
	{
		const Handle f(path, "rb");
		if (!f)
		{
			return std::unexpected(FileNotFound);
		}

	    const i32 s = f.size();
		return s;
	}

	/// Read binary data into caller-provided storage
	static Result<i32> ReadBinary(const char* path, const std::span<u8> output) noexcept
	{
	    const Handle f(path, "rb");
	    if (!f) return std::unexpected(FileNotFound);

	    const i32 fileSize = f.size();
	    if (fileSize <= 0)
	        return std::unexpected(FileReadFailed);

	    if (static_cast<size_t>(fileSize) > output.size())
	        return std::unexpected(OutOfMemory);

	    if (auto res = f.read<u8>(output.first(fileSize)); !res)
	    {
	        return std::unexpected(res.error());
	    }

	    return fileSize;;
	}

	/// Write binary data from caller-provided buffer
	static Result<void> WriteBinary(const char* path, std::span<const u8> data) noexcept
	{
		const Handle f(path, "wb");
		if (!f)
			return std::unexpected(FileNotFound);

		const size_t written = f.write(data.data(), 1, data.size_bytes());
		if (written != data.size_bytes())
			return std::unexpected(FileWriteFailed);

		return {};
	}

    static Result<Vector<u32>> LoadSPV(const char* path)
    {

	    const Handle f(path, "rb");
	    if (!f) return std::unexpected(FileNotFound);

	    const i32 fileSize = f.size();
	    if (fileSize <= 0 || fileSize % sizeof(u32) != 0)
	    {
	        return std::unexpected(InvalidFileFormat);
	    }


	    Vector<u32> buffer(fileSize / sizeof(u32));

	    auto res = f.read<u32>(buffer);
	    if (!res)
	    {
	        return std::unexpected(res.error());
	    }

	    return buffer;
    }

	/// Read text file into caller-provided buffer (null-terminated)
	static Result<i32> ReadText(const char* path, std::span<char> output) noexcept
	{
		Handle f(path, "rb");
		if (!f)
			return std::unexpected(FileNotFound);

		f.seek(0, SEEK_END);
		const i32 fileSize = f.tell();
		if (fileSize <= 0)
			return std::unexpected(FileReadFailed);
		(void)f.rewind();

		const i32 toRead = fileSize;
		if (static_cast<i64>(toRead) + 1 > static_cast<i64>(output.size()))
			return std::unexpected(OutOfMemory); // not enough space for '\0'

		const size_t bytesRead = f.read(output.data(), 1, static_cast<size_t>(toRead));
		if (bytesRead != static_cast<size_t>(toRead))
			return std::unexpected(FileReadFailed);

		output[toRead] = '\0';
		return toRead;
	}

	static Result<Handle> Open(const char* path, const char* mode) noexcept
	{
		Handle f(path, mode);
		if (!f)
			return std::unexpected(FileNotFound);
		return std::move(f);
	}

};
