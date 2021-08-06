#ifndef C_MEMORY_FILE_H
#define C_MEMORY_FILE_H
#include <nbl/system/IFile.h>

namespace nbl::system
{
class CFileView : public IFile
{
public:
	CFileView(const std::filesystem::path& _name, std::underlying_type_t<E_CREATE_FLAGS> _flags) : IFile(nullptr, _flags | ECF_COHERENT | ECF_MAPPABLE), m_name(_name)
	{
	}

	virtual const std::filesystem::path& getFileName() const override
	{
		return m_name;
	}

	void* getMappedPointer() override final { return m_buffer.data(); }
	const void* getMappedPointer() const override final { return m_buffer.data(); }

	size_t getSize() const override final
	{
		return m_buffer.size();
	}
protected:
	void read(system::ISystem::virtual_future_t<size_t>& future, void* buffer, size_t offset, size_t sizeToRead) override final
	{
		if (offset + sizeToRead > m_buffer.size())
		{
			// neet to do future.setResult(0)
		}
		future.get()
		memcpy(buffer, m_buffer.data(), sizeToRead);
		//need to do future.setResult(sizeTowrite)
	}

	size_t write(system::ISystem::virtual_future_t, const void* buffer, size_t offset, size_t sizeToWrite) override final
	{
		if (offset + sizeToWrite > m_buffer.size())
		{
			m_buffer.resize(offset + sizeToWrite);
		}
		memcpy(m_buffer.data() + offset, buffer, sizeToWrite);
		return sizeToWrite;
	}
private:
	std::filesystem::path m_name;
	core::vector<uint8_t> m_buffer;
};

// TODO custom allocator memory file
}

#endif