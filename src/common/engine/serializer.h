#ifndef __SERIALIZER_H
#define __SERIALIZER_H

#include <stdint.h>
#include <type_traits>
#include "tarray.h"
#include "file_zip.h"
#include "tflags.h"
#include "vectors.h"
#include "palentry.h"
#include "name.h"
#include "dictionary.h"

extern bool save_full;

struct FWriter;
struct FReader;
class PClass;
class FFont;
class FSoundID;
union FRenderStyle;
class DObject;
class FTextureID;

inline bool nullcmp(const void *buffer, size_t length)
{
	const char *p = (const char *)buffer;
	for (; length > 0; length--)
	{
		if (*p++ != 0) return false;
	}
	return true;
}

struct NumericValue
{
	enum EType
	{
		NM_invalid,
		NM_signed,
		NM_unsigned,
		NM_float
	} type;

	union
	{
		int64_t signedval;
		uint64_t unsignedval;
		double floatval;
	};

	bool operator !=(const NumericValue &other)
	{
		return type != other.type || signedval != other.signedval;
	}
};


class FSerializer
{

public:
	FWriter *w = nullptr;
	FReader *r = nullptr;
	bool soundNamesAreUnique = false; // While in GZDoom, sound names are unique, that isn't universally true - let the serializer handle both cases with a flag.

	unsigned ArraySize();
	void WriteKey(const char *key);
	void WriteObjects();

private:
	virtual void CloseReaderCustom() {}
public:

	~FSerializer()
	{
		mErrors = 0;	// The destructor may not throw an exception so silence the error checker.
		Close();
	}
	void SetUniqueSoundNames() { soundNamesAreUnique = true; }
	bool OpenWriter(bool pretty = true);
	bool OpenReader(const char *buffer, size_t length);
	bool OpenReader(FCompressedBuffer *input);
	void Close();
	void ReadObjects(bool hubtravel);
	bool BeginObject(const char *name);
	void EndObject();
	bool BeginArray(const char *name);
	void EndArray();
	unsigned GetSize(const char *group);
	const char *GetKey();
	const char *GetOutput(unsigned *len = nullptr);
	FCompressedBuffer GetCompressedOutput();
	// The sprite serializer is a special case because it is needed by the VM to handle its 'spriteid' type.
	virtual FSerializer &Sprite(const char *key, int32_t &spritenum, int32_t *def);
	// This is only needed by the type system.
	virtual FSerializer& StatePointer(const char* key, void* ptraddr, bool *res);
	FSerializer& SerializeMemory(const char* key, void* mem, size_t length);

	FSerializer &StringPtr(const char *key, const char *&charptr);	// This only retrieves the address but creates no permanent copy of the string unlike the regular char* serializer.
	FSerializer &AddString(const char *key, const char *charptr);
	const char *GetString(const char *key);
	FSerializer &ScriptNum(const char *key, int &num);
	bool isReading() const
	{
		return r != nullptr;
	}

	bool isWriting() const
	{
		return w != nullptr;
	}
	
	bool canSkip() const;

	template<class T>
	FSerializer &operator()(const char *key, T &obj)
	{
		return Serialize(*this, key, obj, (T*)nullptr);
	}

	template<class T>
	FSerializer &operator()(const char *key, T &obj, T &def)
	{
		return Serialize(*this, key, obj, save_full? nullptr : &def);
	}

	template<class T>
	FSerializer& operator()(const char* key, T& obj, T* def)
	{
		return Serialize(*this, key, obj, !def || save_full ? nullptr : def);
	}

	template<class T>
	FSerializer &Array(const char *key, T *obj, int count, bool fullcompare = false)
	{
		if (!save_full && fullcompare && isWriting() && nullcmp(obj, count * sizeof(T)))
		{
			return *this;
		}

		if (BeginArray(key))
		{
			if (isReading())
			{
				int max = ArraySize();
				if (max < count) count = max;
			}
			for (int i = 0; i < count; i++)
			{
				Serialize(*this, nullptr, obj[i], (T*)nullptr);
			}
			EndArray();
		}
		return *this;
	}

	template<class T>
	FSerializer &Array(const char *key, T *obj, T *def, int count, bool fullcompare = false)
	{
		if (!save_full && fullcompare && isWriting() && def != nullptr && !memcmp(obj, def, count * sizeof(T)))
		{
			return *this;
		}
		if (BeginArray(key))
		{
			if (isReading())
			{
				int max = ArraySize();
				if (max < count) count = max;
			}
			for (int i = 0; i < count; i++)
			{
				Serialize(*this, nullptr, obj[i], def ? &def[i] : nullptr);
			}
			EndArray();
		}
		return *this;
	}

	template<class T, class Map>
	FSerializer &SparseArray(const char *key, T *obj, int count, const Map &map, bool fullcompare = false)
	{
		if (BeginArray(key))
		{
			int max = count;
			if (isReading())
			{
				max = ArraySize();
			}
			for (int i = 0; i < count; i++)
			{
				if (map[i])
				{
					Serialize(*this, nullptr, obj[i], (T*)nullptr);
					if (--max < 0) break;
				}
			}
			EndArray();
		}
		return *this;
	}

	template<class T>
	FSerializer &Enum(const char *key, T &obj)
	{
		auto val = (typename std::underlying_type<T>::type)obj;
		Serialize(*this, key, val, nullptr);
		obj = (T)val;
		return *this;
	}

	int mErrors = 0;
	int mObjectErrors = 0;
};

FSerializer& Serialize(FSerializer& arc, const char* key, char& value, char* defval);

FSerializer &Serialize(FSerializer &arc, const char *key, bool &value, bool *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, int64_t &value, int64_t *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, uint64_t &value, uint64_t *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, int32_t &value, int32_t *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, uint32_t &value, uint32_t *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, int16_t &value, int16_t *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, uint16_t &value, uint16_t *defval);
#if !defined(__sun) && !defined(_CHAR_IS_SIGNED)
FSerializer &Serialize(FSerializer &arc, const char *key, int8_t &value, int8_t *defval);
#endif
FSerializer &Serialize(FSerializer &arc, const char *key, uint8_t &value, uint8_t *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, double &value, double *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, float &value, float *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, FTextureID &value, FTextureID *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, DObject *&value, DObject ** /*defval*/, bool *retcode = nullptr);
FSerializer &Serialize(FSerializer &arc, const char *key, FName &value, FName *defval);
FSerializer &Serialize(FSerializer &arc, const char *key, FSoundID &sid, FSoundID *def);
FSerializer &Serialize(FSerializer &arc, const char *key, FString &sid, FString *def);
FSerializer &Serialize(FSerializer &arc, const char *key, NumericValue &sid, NumericValue *def);

template<class T>
FSerializer &Serialize(FSerializer &arc, const char *key, T *&value, T **)
{
	DObject *v = static_cast<DObject*>(value);
	Serialize(arc, key, v, nullptr);
	value = static_cast<T*>(v);
	return arc;
}


template<class T, class TT>
FSerializer &Serialize(FSerializer &arc, const char *key, TArray<T, TT> &value, TArray<T, TT> *def)
{
	if (arc.isWriting())
	{
		if (value.Size() == 0 && key) return arc;	// do not save empty arrays
	}
	bool res = arc.BeginArray(key);
	if (arc.isReading())
	{
		if (!res)
		{
			value.Clear();
			return arc;
		}
		value.Resize(arc.ArraySize());
	}
	for (unsigned i = 0; i < value.Size(); i++)
	{
		Serialize(arc, nullptr, value[i], def? &(*def)[i] : nullptr);
	}
	arc.EndArray();
	return arc;
}

template<class T>
FSerializer& Serialize(FSerializer& arc, const char* key, TPointer<T>& value, TPointer<T>* def)
{
	if (arc.isWriting())
	{
		if (value.Data() == nullptr && key) return arc;
	}
	bool res = arc.BeginArray(key);
	if (arc.isReading())
	{
		if (!res || arc.ArraySize() == 0)
		{
			value.Clear();
			return arc;
		}
		value.Alloc();
	}
	if (value.Data())
	{
		Serialize(arc, nullptr, *value, def ? def->Data() : nullptr);
	}
	arc.EndArray();
	return arc;
}


template<int size> 
FSerializer& Serialize(FSerializer& arc, const char* key, FixedBitArray<size>& value, FixedBitArray<size>* def)
{
	return arc.SerializeMemory(key, value.Storage(), value.StorageSize());
}

template<> FSerializer& Serialize(FSerializer& arc, const char* key, PClass*& clst, PClass** def);
template<> FSerializer& Serialize(FSerializer& arc, const char* key, FFont*& font, FFont** def);
template<> FSerializer &Serialize(FSerializer &arc, const char *key, Dictionary *&dict, Dictionary **def);

inline FSerializer &Serialize(FSerializer &arc, const char *key, DVector3 &p, DVector3 *def)
{
	return arc.Array<double>(key, &p[0], def? &(*def)[0] : nullptr, 3, true);
}

inline FSerializer &Serialize(FSerializer &arc, const char *key, DRotator &p, DRotator *def)
{
	return arc.Array<DAngle>(key, &p[0], def? &(*def)[0] : nullptr, 3, true);
}

inline FSerializer &Serialize(FSerializer &arc, const char *key, DVector2 &p, DVector2 *def)
{
	return arc.Array<double>(key, &p[0], def? &(*def)[0] : nullptr, 2, true);
}

template<class T>
inline FSerializer &Serialize(FSerializer &arc, const char *key, TAngle<T> &p, TAngle<T> *def)
{
	return Serialize(arc, key, p.Degrees, def? &def->Degrees : nullptr);
}

inline FSerializer &Serialize(FSerializer &arc, const char *key, PalEntry &pe, PalEntry *def)
{
	return Serialize(arc, key, pe.d, def? &def->d : nullptr);
}

FSerializer &Serialize(FSerializer &arc, const char *key, FRenderStyle &style, FRenderStyle *def);

template<class T, class TT>
FSerializer &Serialize(FSerializer &arc, const char *key, TFlags<T, TT> &flags, TFlags<T, TT> *def)
{
	return Serialize(arc, key, flags.Value, def? &def->Value : nullptr);
}

// Automatic save record registration

struct SaveRecord
{
	const char* GameModule;
	void (*Handler)(FSerializer& arc);

	SaveRecord(const char* nm, void (*handler)(FSerializer& arc));
};

struct SaveRecords
{
	TArray<SaveRecord*> records;

	void RunHandlers(const char* gameModule, FSerializer& arc)
	{
		for (auto record : records)
		{
			if (!strcmp(gameModule, record->GameModule))
			{
				record->Handler(arc);
			}
		}
	}
};

extern SaveRecords saveRecords;

inline SaveRecord::SaveRecord(const char* nm, void (*handler)(FSerializer& arc))
{
	GameModule = nm;
	Handler = handler;
	saveRecords.records.Push(this);
}

FString DictionaryToString(const Dictionary &dict);
Dictionary *DictionaryFromString(const FString &string);

#endif
