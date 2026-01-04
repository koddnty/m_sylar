# address模块接口与使用

## address模块描述

address模块提供对ip地址（4、6）和unix地址的封装，能够与socket模块结合使用，同时提供了网卡信息查询和地址解析等功能。

##  address模块接口部分

### 类关系

```txt
Address -----IPAddress-----IPv4Address
		 |			   |
		 |			   ----IPv6Address
		 |		
         |-----------------UnixAddress
		 |
		 ------------------UnknownAddress
```

### 公共基类 Address

Address类是抽象类，同时提供了不同地址的自动创建函数，以及网卡地址查询和地址解析函数。

Address类实现了比较操作符重载。

#### 虚函数getAddr

``` cpp
virtual sockaddr* getAddr() const = 0;
```

**函数描述**

获取当前Address的sockaddr指针。

#### 虚函数getAddrLen

```cpp
socklen_t getAddrLen() const override;
```

**返回值**

返回当前Address的sockaddr的大小，单位Byte。

#### 虚函数insert

```cpp
virtual std::ostream& insert(std::ostream& os) const = 0;
```

**函数描述**

将Address信息写入到参数os流中，并返回os流。

**返回值**

返回传入的os（std::ostream）流中。

#### getFamily

```cpp
int getFamily() const;
```

**返回值**

返回当前Address的Family。

#### toString

```cpp
std::string toString();
```

**返回值**

#### Create函数

```cpp
static Address::ptr Create(sockaddr* addr, socklen_t path_len = 0);
```

**函数描述** 

该函数将根据addr中的family生成对应的不同类的地址，其中字段path_len将用于Unix的地址长度构建，应当输入unix地址的长度，也可以直接使用``UnixAddress``的构造函数避免地址长度计算。

**返回值**

创建成功后将返回对应封装后的Address类智能指针，若失败将打印日志并返回``nullptr``。

#### Lookup

#### LookupAnyIPAddress

``` cpp
static bool Lookup(std::vector<Address::ptr>& result, const std::string& host, 
                    int family = AF_UNSPEC, int type = 0, int protocol = 0);

static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host, int family = AF_UNSPEC,
                    int type = 0, int protocol = 0);  
```

**函数描述**

`` Lookup``函数将查询`host`*所有*解析的地址并生成对应的address,返回写入到``result``中， 后三个参数将筛选查询到的地址类型。 

`` LookupAnyIPAddress`` 函数使用``Lookup`` 查询后返回一个地址。

**返回值** 

`` Lookup``函数如果查询到数据，返回true, 如果没有查询到任何数据或者查询失败，返回false。

`` LookupAnyIPAddress`` 函数如果查询到数据返回对应address,如果查询失败或者无数据返回nullptr。

#### getInterfaceAddress

``` cpp
static bool getInterfaceAddress(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result, 
                    int family = AF_UNSPEC);

static bool getInterfaceAddress(std::vector<std::pair<Address::ptr, uint32_t>>& result, 
                int family, const std::string& interfaceName);
```

**函数描述**

获取网卡地址信息。

**函数参数**

- result：		**追加写入**网卡名称及其地址，地址长度`` {网卡名称， {网卡地址， 地址长度}}`` 
- family：		对网卡类型进行筛选，筛选符合类型的网卡信息写入`` result`` 
- interfaceName    写入``result`` 特定名称网卡信息。

**返回值**

如果成功查询并有数据返回true，如果失败或无数据返回false。

### IPAddress类 <--- Address

IPAddress类是一个抽象类，继承自Address类。

#### 虚函数networkAddress

```cpp
virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
```

**函数描述**

根据prefix_len获取当前地址的子网地址。

**函数参数**

- prefix_len： 子网掩码前缀长度。

**返回值**

返回获得的计算后的子网地址， 若前缀长度不合法将返回nullptr。

#### 虚函数subnetMask

```cpp
virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;
```

**函数描述** 

根据前缀长度计算子网掩码

**函数参数**

- prefix_len： 子网掩码前缀长度。

**返回值**

返回子网掩码地址值。

#### 虚函数getPort

```cpp
virtual uint16_t getPort() const = 0;
```

**返回值**

获取当前地址的端口。

#### 虚函数setPort

```cpp
virtual void setPort(uint16_t v) = 0;
```

**函数参数**
	- v： 设置当前地址端口

### ipv4类 <--- IPAddress

```cpp
1 IPv4Address();
2 IPv4Address(const sockaddr_in& addr);						// 传入ipv4对应sockaddr
3 IPv4Address(uint32_t address, uint16_t port = 0);			// 传入ipv4地址和端口
4 IPv4Address(const char* address, uint16_t port = 0);		// 传入ipv4地址（如192.168.0.1，自动解析）和端口
```

#### broadcastAddress

```cpp
IPAddress::ptr broadcastAddress(uint32_t prefix_len);
```

**函数描述**

获取当前子网广播地址。

**函数参数**

- prefix_len： 子网掩码前缀长度。

**返回值**

返回子网广播地址。

### ipv6类 <--- IPAddress

```cpp
    1 IPv6Address();
    2 IPv6Address(const sockaddr_in6& addr);					// 传入sockaddr
    3 IPv6Address(const char* address, uint16_t port = 0);		// 传入ipv6地址和对应端口
    4 IPv6Address(const uint8_t* addr, uint16_t port = 0);		// 传入ipv6地址和对应端口
```

无特殊实现

### UnixAddress  <--- Address

**构造函数**

```cpp
    1 UnixAddress();	
    2 UnixAddress(const std::string& path);			// 传入sockaddr中unix地址
    3 UnixAddress(const sockaddr_un& addr, int path_len);		// 传入sockaddr和其unix地址长度
```

#### setAddrLen

```cpp
void setAddrLen(socklen_t len);
```

**函数描述** 

设置unix类型sockaddr的地址长度。若通过构造函数2构造则自动设置sockaddr地址长度。

## 简单使用实例：

[address_test](./../../tests/address_test.cc)
[socket_test](../../tests/test_socket.cc)