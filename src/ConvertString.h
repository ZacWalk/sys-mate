typedef WCHAR **LPLPWSTR;

class CStringConvert : public CObject
{
public:

	wchar_t *m_wcstr;

	CStringConvert(LPCSTR mbstr)
	{
		int count = ::MultiByteToWideChar(CP_ACP, 0, mbstr, -1, m_wcstr, 0);
		::MultiByteToWideChar(CP_ACP, 0, mbstr, -1, m_wcstr = new wchar_t[count], count);
	}

	~CStringConvert()
	{
		delete m_wcstr;
	}

	operator LPCWSTR()
	{
		return m_wcstr;
	}

	operator LPWSTR()
	{
		return m_wcstr;
	}

	operator LPLPWSTR()
	{
		return &m_wcstr;
	}
};