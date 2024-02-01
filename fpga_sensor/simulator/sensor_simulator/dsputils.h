#ifndef DSPUTILS_H
#define DSPUTILS_H

#include <stdint.h>
#include <QDebug>

template <typename T> class Array {
    const T initValue = T();
    T * _list;
    int _size;
    int _count;
public:
    T * data() {
        return _list;
    }
    void clear() {
        if (_list)
            delete[] _list;
        _list = nullptr;
        _size = 0;
        _count = 0;
    }
    void reserve(int size) {
        if (_size < size) {
            resize(size);
        }
    }
    // set array to "size" elements of specified value
    void init(int size, T value) {
        resize(size);
        _count = size;
        for (int i = 0; i < _count; i++) {
            _list[i] = value;
        }
    }
    void resize(int size) {
        if (size <= 0) {
            clear();
        } else if (_size == 0) {
            // create new list
            _list = new T[size];
            for (int i = 0; i < size; i++)
                _list[i] = initValue;
            _size = size;
            _count = 0;
        } else if (size != _size) {
            // we have some items in list
            T * newList = new T[size];
            // copy existing items to new list
            for (int i = 0; i < _count && i < _size && i < size; i++) {
                newList[i] = _list[i];
            }
            for (int i = _count; i < _size && i < size; i++) {
                newList[i] = initValue;
            }
            delete [] _list;
            _list = newList;
            _size = size;
            if (_count > _size)
                _count = size;
        }
    }
    Array() : _list(nullptr), _size(0), _count(0) {}
    Array(const Array<T> & v) : _list(nullptr), _size(0), _count(0) {
        addAll(v);
    }
    ~Array() {
        clear();
    }
    int size() const { return _size; }
    int length() const { return _count; }
    T& operator[] (int index) {
        Q_ASSERT(index >= 0 && index < _count);
        return _list[index];
    }
    const T& operator[] (int index) const {
        Q_ASSERT(index >= 0 && index < _count);
        return _list[index];
    }
//    void add(const T & item) {
//        if (_count >= _size)
//            resize(_size == 0 ? 32 : _size * 2);
//        _list[_count++] = item;
//    }
    void add(const T & item) {
        if (_count >= _size)
            resize(_size == 0 ? 32 : _size * 2);
        _list[_count++] = item;
    }
    void addAll(const Array<T> & v) {
        reserve(_count + v.length());
        for (int i = 0; i < v.length(); i++) {
            add(v[i]);
        }
    }
    void operator = (const Array<T> & v) {
        clear();
        addAll(v);
    }
};

// sine lookup table, with phase and value quantized according number of bits specified during initialization
// implementation may use 1/4 of table entries, with flipping and inverting, to get value for any phase
class SinTable {
    int* table;
    int tableSizeBits;
    int tableSize;
    int valueBits;
    int phaseBits;
public:
    SinTable(int tableSizeBits = 10, int valueBits = 9, double scale = 1.0, int phaseBits=32);
    ~SinTable();
    // returns table size bits, e.g. 10 for 1024 entries table
    int getTableSizeBits() const { return tableSizeBits; }
    // returns table size as number of table entries
    int getTableSize() const { return tableSize; }
    // returns value range in bits
    int getValueBits() const { return valueBits; }
    // initialize lookup table of desired table size and value range
    void init(int tableSizeBits, int valueBits, double scale = 1.0, int phaseBits=32);

    // get sine table value from full table entry index, using only 1/4 of table entries algorithm
    int get(int fullTableIndex) const;
    // get sine table value from phase, using only 1/4 of table
    int getForPhase(uint64_t phase) const;
    // get sine table value from phase, using full phase
    int getForPhaseFull(uint32_t phase) const;
    // get table entry by index
    int operator [](int index) const;

    // generate verilog source code with sine table
    bool generateVerilog(const char * filePath);
    // generate init file for $readmemh
    bool generateMemInitFile(const char * filePath);
    // generate verilog source code for verification of sine table
    bool generateVerilogTestBench(const char * filePath);
    // generate verilog source code with sin & cos table
    bool generateSinCosVerilog(const char * filePath);
    // generate verilog source code with verificatoin of sin & cos table
    bool generateSinCosVerilogTestBench(const char * filePath);
};

class SimDevice {
public:
    SimDevice() {}
    virtual ~SimDevice() {}
    // prepare new value
    virtual void onUpdate() {}
    // reset
    virtual void onReset() {}
    // propagate new value to output
    virtual void onClock() {}
};

class SimReg;
class SimClock {
protected:
    SimDevice ** _devices;
    int _devicesSize;
    int _devicesCount;
public:
    SimClock() : _devices(nullptr), _devicesSize(0), _devicesCount(0) {}
    virtual ~SimClock();
    virtual void add(SimDevice * reg);
    virtual void onUpdate();
    virtual void onClock();
    virtual void onReset();
};

class SimValue : public SimDevice {
protected:
    const SimValue * _connectedValue;
public:
    SimValue() {}
    ~SimValue() override {
    }
    void connect(SimValue * value) {
        _connectedValue = value;
    }
    // return number of bits in value
    virtual int bits() const {
        return 1;
    }
    // return true if value is signed
    virtual bool isSigned() const {
        return false;
    }
    // get input value
    virtual int64_t peek() const = 0;
    // get output value (for wire, it's the same as input value)
    virtual int64_t get() const { return peek(); }
    // set new value -- when has connected value, use connectedValue.set() instead
    virtual void set(int64_t value) = 0;
    // fit value into range, with sign extension if necessary
    virtual int64_t fixRange(int64_t value) const {
        return value & 1;
    }
    operator int64_t() const {
        return get();
    }
    SimValue & operator =(const SimValue& v) {
        set(v);
        return *this;
    }
    SimValue & operator =(int64_t v) {
        set(v);
        return *this;
    }
};

class SimBit : public SimValue {
protected:
    uint8_t _value;
public:
    SimBit(int64_t initValue = 0) : _value(initValue != 0 ? 1 : 0) {}
    ~SimBit() override {}
    // get input value
    int64_t peek() const override {
        return (_connectedValue ? _connectedValue->get() : _value) & 1;
    }
    // set new value -- when has connected value, use connectedValue.set() instead
    void set(int64_t value) override {
        _value = value & 1;
    }
    // fit value into range, with sign extension if necessary
    int64_t fixRange(int64_t value) const override {
        return value & 1;
    }
};

class SimBitReg : public SimBit {
protected:
    uint8_t _outValue;
    uint8_t _resetValue;
    SimBit _ce;
    SimBit _reset;
public:
    SimBitReg(SimClock& clock, int64_t initValue = 0) : SimBit(initValue), _outValue(initValue), _resetValue(initValue),
        _ce(1), _reset(0)
    {
        clock.add(this);
    }
    // access CE input wire
    SimBit & ce() { return _ce; }
    // access RESET input wire
    SimBit & reset() { return _reset; }
    // return value from register output
    int64_t get() const override {
        return fixRange(_outValue);
    }
    void onReset() override {
        _outValue = _resetValue;
    }
    // propagate input value to register output
    void onClock() override {
        if (_reset.get())
            _outValue = _resetValue;
        else
            _outValue = peek() ? 1 : 0;
    }
};

// value with limited range of specified number of bits, signed or unsigned
// can be directly used as verilog wire
class SimBus : public SimValue {
protected:
    int _bits;
    bool _signed;
    int64_t _mask;
    int64_t _value;
    SimBus(int bits = 1, bool isSigned = false, int64_t initValue = 0) :
        SimValue(),
        _bits(bits), _signed(isSigned),
        _mask((1ULL<<bits)-1), _value(initValue) {
    }
    ~SimBus() override {
    }
    // return number of bits in value
    int bits() const override {
        return _bits;
    }
    // return true if value is signed
    bool isSigned() const override {
        return _signed;
    }
    // get input value
    int64_t peek() const override {
        return fixRange(_connectedValue ? _connectedValue->get() : _value);
    }
    // set new value -- when has connected value, use connectedValue.set() instead
    void set(int64_t value) override {
        assert(_connectedValue == nullptr);
        _value = fixRange(value);
    }
    // fit value into range, with sign extension if necessary
    int64_t fixRange(int64_t value) const override {
        // width mask and sign extension
        if (_signed) {
            if (value & ((_mask+1)>>1)) {
                // negative
                return value | (~_mask);
            } else {
                // positive
                return value & _mask;
            }
        } else {
            return value & _mask;
        }
    }
};

// Register, updated by clock
// can be directly used as verilog reg
class SimReg : public SimBus {
protected:
    int64_t _outValue;
    int64_t _resetValue;
    SimBit _ce;
    SimBit _reset;
public:
    SimReg(SimClock& clock, int bits = 1, bool isSigned = false, int64_t resetValue=0) :
        SimBus(bits, isSigned, resetValue),
        _outValue(resetValue), _resetValue(resetValue), _ce(1), _reset(0) {
        clock.add(this);
    }
    // access CE (clock enable) input wire
    SimBit & ce() { return _ce; }
    // access RESET input wire
    SimBit & reset() { return _reset; }
    //
    int64_t get() const override {
        return fixRange(_outValue);
    }
    void onReset() override {
        _outValue = _resetValue;
    }
    void onClock() override {
        if (_reset.get())
            _outValue = _resetValue;
        else if (_ce.get())
            _outValue = peek();
    }
};

void circleCenter(int &outx, int &outy, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3);
void circleCenter3(int &outx, int &outy, int x0, int y0, int x1, int y1, int x2, int y2);

/*

  SimReg r(10);


 */

void testCORDIC();

#endif // DSPUTILS_H
