//
// dev.c
//
// Copyright (c) 2001 Michael Ringgaard. All rights reserved.
//
// Device Manager
//

#include <os/krnl.h>

struct unit *units;
struct bus *buses;

struct binding *bindtab;
int num_bindings;

struct dev *devtab[MAX_DEVS];
unsigned int num_devs = 0;

struct bus *add_bus(struct unit *self, unsigned long bustype, unsigned long busno)
{
  struct bus *bus;
  struct bus *b;

  // Create new bus
  bus = (struct bus *) kmalloc(sizeof(struct bus));
  if (!bus) return NULL;
  memset(bus, 0, sizeof(struct bus));

  bus->self = self;
  if (self) bus->parent = self->bus;
  bus->bustype = bustype;
  bus->busno = busno;

  // Add bus as a bridge on the parent bus
  if (bus->parent)
  {
    if (bus->parent->bridges)
    {
      b = bus->parent->bridges;
      while (b->sibling) b = b->sibling;
      b->sibling = bus;
    }
    else
      bus->parent->bridges = bus;
  }

  // Add bus to bus list
  if (buses)
  {
    b = buses;
    while (b->next) b = b->next;
    b->next = bus;
  }
  else
    buses = bus;

  return bus;
}

struct unit *add_unit(struct bus *bus, unsigned long classcode, unsigned long unitcode, unsigned long unitno)
{
  struct unit *unit;
  struct unit *u;

  // Create new unit
  unit = (struct unit *) kmalloc(sizeof(struct unit));
  if (!unit) return NULL;
  memset(unit, 0, sizeof(struct unit));

  unit->bus = bus;
  unit->classcode = classcode;
  unit->unitcode = unitcode;
  unit->unitno = unitno;

  unit->classname = "";
  unit->vendorname = "";
  unit->productname = "";

  // Add unit to bus
  if (bus)
  {
    if (bus->units)
    {
      u = bus->units;
      while (u->sibling) u = u->sibling;
      u->sibling = unit;
    }
    else
      bus->units = unit;
  }

  // Add unit to unit list
  if (units)
  {
    u = units;
    while (u->next) u = u->next;
    u->next = unit;
  }
  else
    units = unit;

  return unit;
}

struct resource *add_resource(struct unit *unit, unsigned short type, unsigned short flags, unsigned long start, unsigned long len)
{
  struct resource *res;
  struct resource *r;

  // Create new resource
  res = (struct resource *) kmalloc(sizeof(struct resource));
  if (!res) return NULL;
  memset(res, 0, sizeof(struct resource));

  res->type = type;
  res->flags = flags;
  res->start = start;
  res->len = len;

  // Add resource to unit resource list
  if (unit->resources)
  {
    r = unit->resources;
    while (r->next) r = r->next;
    r->next = res;
  }
  else
    unit->resources = res;

  return res;
}

struct resource *get_unit_resource(struct unit *unit, int type, int num)
{
  struct resource *res = unit->resources;

  while (res)
  {
    if (res->type == type)
    {
      if (num == 0)
	return res;
      else
	num--;
    }

    res = res->next;
  }

  return NULL;
}

int get_unit_irq(struct unit *unit)
{
  struct resource *res = get_unit_resource(unit, RESOURCE_IRQ, 0);
  
  if (res) return res->start;
  return -1;
}

int get_unit_iobase(struct unit *unit)
{
  struct resource *res = get_unit_resource(unit, RESOURCE_IO, 0);
  
  if (res) return res->start;
  return -1;
}

void *get_unit_membase(struct unit *unit)
{
  struct resource *res = get_unit_resource(unit, RESOURCE_MEM, 0);
  
  if (res) return (void *) (res->start);
  return NULL;
}

char *get_unit_name(struct unit *unit)
{
  if (unit->productname && *unit->productname) return unit->productname;
  if (unit->classname && *unit->classname) return unit->classname;
  return "unknown";
}

struct unit *lookup_unit(struct unit *start, unsigned long unitcode, unsigned long unitmask)
{
  struct unit *unit;
  
  if (start)
    unit = start->next;
  else
    unit = units;

  while (unit)
  {
    if ((unit->unitcode & unitmask) == unitcode) return unit;
    unit = unit->next;
  }

  return NULL;
}

struct unit *lookup_unit_by_class(struct unit *start, unsigned long classcode, unsigned long classmask)
{
  struct unit *unit;
  
  if (start)
    unit = start->next;
  else
    unit = units;

  while (unit)
  {
    if ((unit->classcode & classmask) == classcode) return unit;
    unit = unit->next;
  }

  return NULL;
}

void enum_host_bus()
{
  struct bus *host_bus;
  struct unit *pci_host_bridge;
  unsigned long unitcode;
  struct bus *pci_root_bus;
  struct unit *isa_bridge;
  struct bus *isa_bus;

  // Create host bus
  host_bus = add_bus(NULL, BUSTYPE_HOST, 0);

  unitcode = get_pci_hostbus_unitcode();
  if (unitcode)
  {
    // Enumerate PCI buses
    pci_host_bridge = add_unit(host_bus, PCI_HOST_BRIDGE, unitcode, 0);
    pci_root_bus = add_bus(pci_host_bridge, BUSTYPE_PCI, 0);

    enum_pci_bus(pci_root_bus);
  }
  else
  {
    // Enumerate ISA bus using PnP
    isa_bridge = add_unit(host_bus, PCI_ISA_BRIDGE, 0, 0);
    isa_bus = add_bus(isa_bridge, BUSTYPE_ISA, 0);

    enum_isapnp(isa_bus);
  }
}

static void parse_bindings()
{
  struct section *sect;
  struct property *prop;
  struct binding *bind;
  char buf[128];
  char *p;
  char *q;
  int n;

  // Parse driver bindings
  sect = find_section(krnlcfg, "bindings");
  if (!sect) return;
  num_bindings = get_section_size(sect);
  if (!num_bindings) return;

  bindtab = (struct binding *) kmalloc(num_bindings * sizeof(struct binding));
  memset(bindtab, 0, num_bindings * sizeof(struct binding));

  n = 0;
  prop = sect->properties;
  while (prop)
  {
    bind = &bindtab[n];

    strcpy(buf, prop->name);
    
    p = q = buf;
    while (*q && *q != ' ') q++;
    if (!*q) continue;
    *q++ = 0;

    if (strcmp(p, "pci") == 0)
      bind->bustype = BUSTYPE_PCI;
    else if (strcmp(p, "isa") == 0)
      bind->bustype = BUSTYPE_ISA;

    while (*q == ' ') q++;
    p = q;
    while (*q && *q != ' ') q++;
    if (!*q) continue;
    *q++ = 0;

    if (strcmp(p, "class") == 0)
      bind->bindtype = BIND_BY_CLASSCODE;
    else if (strcmp(p, "unit") == 0)
      bind->bindtype = BIND_BY_UNITCODE;

    while (*q == ' ') q++;

    while (*q)
    {
      unsigned long digit;
      unsigned long mask;

      mask = 0xF;
      if (*q >= '0' && *q <= '9')
	digit = *q - '0';
      else if (*q >= 'A' && *q <= 'F')
	digit = *q - 'A' + 10;
      else if (*q >= 'a' && *q <= 'f')
	digit = *q - 'a' + 10;
      else
      {
	digit = 0;
	mask = 0;
      }

      bind->code = (bind->code << 4) | digit;
      bind->mask = (bind->mask << 4) | mask;
      q++;
    }

    bind->module = prop->value;

    //kprintf("binding %c %08X %08X %s\n", bind->type, bind->code, bind->mask, bind->module);
    prop = prop->next;
    n++;
  }
}

static struct binding *find_binding(struct unit *unit)
{
  int n;

  for (n = 0; n < num_bindings; n++)
  {
    struct binding *bind = &bindtab[n];

    if (bind->bustype == unit->bus->bustype)
    {
      if (bind->bindtype == BIND_BY_CLASSCODE)
      {
	if ((unit->classcode & bind->mask) == bind->code) return bind;
      }

      if (bind->bindtype == BIND_BY_UNITCODE)
      {
	if ((unit->unitcode & bind->mask) == bind->code) return bind;
      }
    }
  }

  return NULL;
}

static void *get_driver_entry(char *module, char *defentry)
{
  char modfn[MAXPATH];
  char *entryname;
  hmodule_t hmod;

  strcpy(modfn, module);
  entryname = strchr(modfn, '!');
  if (entryname)
    *entryname++ = 0;
  else 
    entryname = defentry;

  hmod = load(modfn);
  if (!hmod) return NULL;

  return resolve(hmod, entryname);
}

static void install_driver(struct unit *unit, struct binding *bind)
{
  int (*entry)(struct unit *unit);
  int rc;

  entry = get_driver_entry(bind->module, "install");
  if (!entry)
  {
    kprintf("warning: unable to load driver %s for unit '%s'\n", bind->module, get_unit_name(unit));
    return;
  }

  rc = entry(unit);
  if (rc < 0)
  {
    kprintf("warning: error %d installing driver %s for unit '%s'\n", rc, bind->module, get_unit_name(unit));
    return;
  }
}

static void bind_units()
{
  struct unit *unit = units;

  while (unit)
  {
    struct binding *bind = find_binding(unit);
    if (bind) install_driver(unit, bind);
    unit = unit->next;
  }
}

static void install_legacy_drivers()
{
  struct section *sect;
  struct property *prop;
  int (*entry)(char *opts);
  int rc;

  sect = find_section(krnlcfg, "drivers");
  if (!sect) return;

  prop = sect->properties;
  while (prop)
  {
    entry = get_driver_entry(prop->name, "install");
    if (entry)
    {
      rc = entry(prop->value);
      if (rc < 0) kprintf("warning: error %d installing driver %s\n", rc, prop->name);      
    }
    else
      kprintf("warning: unable to load driver %s\n", prop->name);

    prop = prop->next;
  }
}

void install_drivers()
{
  // Parse driver binding database
  parse_bindings();

  // Match bindings to units
  bind_units();

  // Install legacy drivers
  install_legacy_drivers();
}

struct dev *device(devno_t devno)
{
  if (devno < 0 || devno >= num_devs) return NULL;
  return devtab[devno];
}

devno_t dev_make(char *name, struct driver *driver, struct unit *unit, void *privdata)
{
  struct dev *dev;
  devno_t devno;
  char *p;
  unsigned int n, m;
  int exists;

  if (num_devs == MAX_DEVS) panic("too many devices");

  dev = (struct dev *) kmalloc(sizeof(struct dev));
  if (!dev) return NODEV;
  memset(dev, 0, sizeof(struct dev));

  strcpy(dev->name, name);
  
  p = dev->name;
  while (p[0] && p[1]) p++;
  if (*p == '#')
  {
    n = 0;
    while (1)
    {
      sprintf(p, "%d", n);
      exists = 0;
      for (m = 0; m < num_devs; m++) 
      {
	if (strcmp(devtab[m]->name, dev->name) == 0) 
	{
	  exists = 1;
	  break;
	}
      }

      if (!exists) break;
      n++;
    }
  }

  dev->driver = driver;
  dev->unit = unit;
  dev->privdata = privdata;
  dev->refcnt = 0;

  if (unit) unit->dev = dev;

  devno = num_devs++;
  devtab[devno] = dev;
  
  return devno;
}

devno_t dev_open(char *name)
{
  devno_t devno;

  for (devno = 0; devno < num_devs; devno++)
  {
    if (strcmp(devtab[devno]->name, name) == 0)
    {
      devtab[devno]->refcnt++;
      return devno;
    }
  }

  return NODEV;
}

int dev_close(devno_t devno)
{
  if (devno < 0 || devno >= num_devs) return -ENODEV;
  if (devtab[devno]->refcnt == 0) return -EPERM;
  devtab[devno]->refcnt--;
  return 0;
}

int dev_ioctl(devno_t devno, int cmd, void *args, size_t size)
{
  struct dev *dev;

  if (devno < 0 || devno >= num_devs) return -ENODEV;
  dev = devtab[devno];
  if (!dev->driver->ioctl) return -ENOSYS;

  return dev->driver->ioctl(dev, cmd, args, size);
}

int dev_read(devno_t devno, void *buffer, size_t count, blkno_t blkno)
{
  struct dev *dev;

  if (devno < 0 || devno >= num_devs) return -ENODEV;
  dev = devtab[devno];
  if (!dev->driver->read) return -ENOSYS;

  return dev->driver->read(dev, buffer, count, blkno);
}

int dev_write(devno_t devno, void *buffer, size_t count, blkno_t blkno)
{
  struct dev *dev;

  if (devno < 0 || devno >= num_devs) return -ENODEV;
  dev = devtab[devno];
  if (!dev->driver->read) return -ENOSYS;

  return dev->driver->write(dev, buffer, count, blkno);
}

int dev_attach(devno_t devno, struct netif *netif, int (*receive)(struct netif *netif, struct pbuf *p))
{
  struct dev *dev;

  if (devno < 0 || devno >= num_devs) return -ENODEV;
  dev = devtab[devno];
  if (!dev->driver->attach) return -ENOSYS;

  dev->netif = netif;
  dev->receive = receive;
  return dev->driver->attach(dev, &netif->hwaddr);
}

int dev_detach(devno_t devno)
{
  struct dev *dev;
  int rc;

  if (devno < 0 || devno >= num_devs) return -ENODEV;
  dev = devtab[devno];

  if (dev->driver->detach) 
    rc = dev->driver->detach(dev);
  else
    rc = 0;

  dev->netif = NULL;
  dev->receive = NULL;

  return rc;
}

int dev_transmit(devno_t devno, struct pbuf *p)
{
  struct dev *dev;

  if (devno < 0 || devno >= num_devs) return -ENODEV;
  dev = devtab[devno];
  if (!dev->driver->transmit) return -ENOSYS;

  return dev->driver->transmit(dev, p);
}

int dev_receive(devno_t devno, struct pbuf *p)
{
  struct dev *dev;

  if (devno < 0 || devno >= num_devs) return -ENODEV;
  dev = devtab[devno];
  if (!dev->receive) return -ENOSYS;

  return dev->receive(dev->netif, p);
}
