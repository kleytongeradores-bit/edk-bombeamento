-- Schema básico para o sistema de bombeamento EDK
-- Execute no SQL Editor do Supabase

-- Tabela principal de status (o ESP32 atualiza, o app lê)
create table if not exists public.status (
  id uuid primary key default gen_random_uuid(),
  device_id text not null unique,
  pump_running boolean default false,
  mode text default 'AUTOMATICO',
  level_percent integer,
  b1 boolean,
  b2 boolean,
  b3 boolean,
  b4 boolean,
  b5 boolean,
  alarm_active boolean default false,
  alarm_message text,
  updated_at timestamptz default now()
);

-- Tabela de comandos (o app grava, o ESP32 lê e apaga)
create table if not exists public.commands (
  id uuid primary key default gen_random_uuid(),
  device_id text not null,
  command text not null,          -- LIGAR, DESLIGAR, MODO
  mode text,                      -- MANUAL ou AUTOMATICO (quando command = MODO)
  created_at timestamptz default now(),
  executed boolean default false
);

-- Habilita Realtime (opcional, mas útil)
alter publication supabase_realtime add table public.status;
alter publication supabase_realtime add table public.commands;

-- RLS básico (ajuste conforme sua necessidade de autenticação)
alter table public.status enable row level security;
alter table public.commands enable row level security;

-- Política temporária aberta (apenas para testes — restrinja depois)
create policy "Allow all for now" on public.status for all using (true);
create policy "Allow all for now" on public.commands for all using (true);

-- Depois você pode criar políticas por usuário/device_id
