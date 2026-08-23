#include "config.h"

namespace NYT::NBus::NUcx {

void TBusConfig::Register(TRegistrar registrar)
{
    registrar.Parameter("transports", &TThis::Transports)
        .Default("rc,dc,ud,sm");
}

void TBusClientConfig::Register(TRegistrar registrar)
{
    registrar.Parameter("address", &TThis::Address)
        .Default();
}

void TBusServerConfig::Register(TRegistrar registrar)
{
    registrar.Parameter("port", &TThis::Port)
        .Default();
}

} // namespace NYT::NBus::NUcx
