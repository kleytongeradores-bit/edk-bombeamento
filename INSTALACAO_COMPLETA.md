# Instalação Completa - EDK Elétrica Bombeamento

Guia passo a passo para deixar o sistema funcionando para os clientes (frontend publicado + Supabase + ESP32).

---

## 1. Criar o projeto no Supabase

1. Acesse [supabase.com](https://supabase.com) e crie uma conta (grátis).
2. Clique em **New Project**.
3. Escolha um nome (ex.: `edk-bombeamento`), senha do banco e região mais próxima.
4. Aguarde a criação do projeto (1–2 minutos).

### Executar o schema

1. No menu lateral, vá em **SQL Editor**.
2. Clique em **New query**.
3. Cole todo o conteúdo do arquivo `supabase/schema.sql`.
4. Clique em **Run**.
5. Confirme que as tabelas `status` e `commands` foram criadas (menu **Table Editor**).

### Pegar as chaves

1. Vá em **Project Settings → API**.
2. Copie e guarde:
   - **Project URL** (ex.: `https://abcdefgh.supabase.co`)
   - **anon public** key (começa com `eyJ...`)

---

## 2. Publicar o frontend no GitHub Pages

1. Crie uma conta no [GitHub](https://github.com) se ainda não tiver.
2. Crie um novo repositório:
   - Nome sugerido: `edk-bombeamento`
   - Pode ser **privado**
3. Faça o upload de toda a pasta `edk-bombeamento` (ou use Git).
4. Vá em **Settings → Pages**.
5. Em **Source**:
   - Branch: `main`
   - Folder: `/frontend`
6. Clique em **Save**.
7. Em alguns minutos o app estará disponível em:
   `https://SEU_USUARIO.github.io/edk-bombeamento/`

Anote esse endereço — é o link que você envia para os clientes.

---

## 3. Configurar e gravar o ESP32

1. Abra o Arduino IDE.
2. Instale a biblioteca **ArduinoJson** (versão 6.x) pelo Gerenciador de Bibliotecas.
3. Abra o arquivo `firmware/EDK_Bombeamento_ESP32.ino`.
4. No topo do código, preencha:

```cpp
const char* STA_SSID     = "NOME_DA_SUA_REDE";
const char* STA_PASSWORD = "SENHA_DA_REDE";

const char* SUPABASE_URL = "https://SEU_PROJETO.supabase.co";
const char* SUPABASE_KEY = "SUA_ANON_KEY";
const char* DEVICE_ID    = "bomba-casa-01";   // escolha um ID único
```

5. Deixe `USE_AP_MODE = false`.
6. Selecione a placa ESP32 e a porta correta.
7. Faça o **Upload**.
8. Abra o Serial Monitor (115200 baud) e confirme que conectou no Wi-Fi e está enviando status (aparecem linhas com código 201 ou 200).

---

## 4. Configurar o app (frontend)

1. Abra o link do GitHub Pages no celular ou computador.
2. Clique em **Configurar conexão**.
3. Selecione o modo **Supabase**.
4. Preencha:
   - **Supabase URL** → a URL copiada
   - **Supabase Anon Key** → a chave copiada
   - **Device ID** → o mesmo valor usado no firmware (ex.: `bomba-casa-01`)
5. Clique em **Salvar e conectar**.

O app deve mostrar o status real da bomba e das boias em poucos segundos.

---

## 5. Testar o sistema completo

1. No app, clique em **Atualizar agora**.
2. Verifique se os valores das boias e da bomba aparecem.
3. Coloque o sistema em modo **Manual**.
4. Teste o botão **Solicitar LIGAR** e **Solicitar DESLIGAR**.
5. Confirme no Serial Monitor do ESP32 que o comando chegou e foi executado.
6. Volte para modo **Automático** e teste as condições das boias.

---

## 6. Entregar para os clientes

- Envie apenas o link do GitHub Pages.
- Oriente o cliente a abrir o link, ir em **Configurar conexão** e preencher os dados do Supabase + Device ID (você pode criar um Device ID diferente para cada cliente/instalação).
- O cliente não precisa instalar nada. Funciona no navegador do celular.

---

## Observações importantes

- O ESP32 precisa de internet para sincronizar com o Supabase.
- As proteções de segurança continuam no ESP32 (não liga sem água na cisterna, desliga no nível máximo etc.).
- Para vários clientes, use um `DEVICE_ID` diferente para cada instalação.
- Recomenda-se deixar o repositório GitHub **privado**.

---

## Problemas comuns

| Problema | Solução |
|----------|---------|
| App mostra "Nenhum status encontrado" | Verifique se o `DEVICE_ID` é exatamente igual no firmware e no app |
| ESP32 não envia status | Confirme Wi-Fi e se a URL/key do Supabase estão corretas no código |
| Comando não chega | Olhe o Serial Monitor e a tabela `commands` no Supabase |
| Página do GitHub Pages não abre | Aguarde alguns minutos ou force refresh (Ctrl+F5) |

Pronto. Com esses passos o sistema fica publicado e acessível pelos clientes.
